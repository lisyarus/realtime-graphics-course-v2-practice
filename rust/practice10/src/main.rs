use std::f32::consts::PI;
use std::fs;
use std::mem;
use std::path::Path;
use std::time::Instant;

use glam::{Mat4, Vec3, Vec4};
use wgpu::util::DeviceExt;
use wgpu_app::{AppConfig, KeyCode, WgpuApp, WgpuState, run};

const PROJECT_ROOT: &str = env!("CARGO_MANIFEST_DIR");

#[repr(C)]
#[derive(Clone, Copy, bytemuck::Pod, bytemuck::Zeroable)]
struct CameraUniforms {
    view_projection: Mat4,
    position: Vec3,
    padding: f32,
}

#[repr(C)]
#[derive(Clone, Copy, bytemuck::Pod, bytemuck::Zeroable)]
struct Vertex {
    position: [f32; 3],
    normal: [f32; 3],
    texcoord: [f32; 2],
    tangent: [f32; 3],
}

fn load_shader_module(
    device: &wgpu::Device,
    path: impl AsRef<Path>,
) -> std::io::Result<wgpu::ShaderModule> {
    let source = fs::read_to_string(path)?;
    Ok(device.create_shader_module(wgpu::ShaderModuleDescriptor {
        label: None,
        source: wgpu::ShaderSource::Wgsl(source.into()),
    }))
}

fn load_buffer<T: bytemuck::Pod>(
    device: &wgpu::Device,
    data: &[T],
    usage: wgpu::BufferUsages,
) -> wgpu::Buffer {
    device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
        label: None,
        contents: bytemuck::cast_slice(data),
        usage: wgpu::BufferUsages::COPY_DST | usage,
    })
}

fn load_texture(
    device: &wgpu::Device,
    queue: &wgpu::Queue,
    path: impl AsRef<Path>,
    format: wgpu::TextureFormat,
) -> wgpu::Texture {
    let image = image::open(path)
        .expect("failed to load texture")
        .into_rgba8();
    let (width, height) = image.dimensions();
    let texture = device.create_texture(&wgpu::TextureDescriptor {
        label: Some("albedo texture"),
        size: wgpu::Extent3d {
            width,
            height,
            depth_or_array_layers: 1,
        },
        mip_level_count: 1,
        sample_count: 1,
        dimension: wgpu::TextureDimension::D2,
        format,
        usage: wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST,
        view_formats: &[],
    });

    queue.write_texture(
        wgpu::TexelCopyTextureInfo {
            texture: &texture,
            mip_level: 0,
            origin: wgpu::Origin3d::ZERO,
            aspect: wgpu::TextureAspect::All,
        },
        image.as_raw(),
        wgpu::TexelCopyBufferLayout {
            offset: 0,
            bytes_per_row: Some(4 * width),
            rows_per_image: Some(height),
        },
        wgpu::Extent3d {
            width,
            height,
            depth_or_array_layers: 1,
        },
    );

    texture
}

fn generate_sphere(radius: f32, quality: i32) -> (Vec<Vertex>, Vec<u32>) {
    let mut vertices = Vec::new();

    for latitude in -quality..=quality {
        for longitude in 0..=4 * quality {
            let lat = (latitude as f32 * PI) / (2.0 * quality as f32);
            let lon = (longitude as f32 * PI) / (2.0 * quality as f32);
            let normal = Vec3::new(lat.cos() * lon.cos(), lat.sin(), lat.cos() * lon.sin());
            let tangent = Vec3::new(-lat.cos() * lon.sin(), 0.0, lat.cos() * lon.cos());

            vertices.push(Vertex {
                position: (normal * radius).to_array(),
                normal: normal.to_array(),
                texcoord: [
                    longitude as f32 / (4 * quality) as f32,
                    0.5 - latitude as f32 / (2 * quality) as f32,
                ],
                tangent: tangent.to_array(),
            });
        }
    }

    let mut indices = Vec::new();
    for latitude in 0..2 * quality {
        for longitude in 0..4 * quality {
            let i0 = latitude * (4 * quality + 1) + longitude;
            let i1 = (latitude + 1) * (4 * quality + 1) + longitude;
            let i2 = latitude * (4 * quality + 1) + longitude + 1;
            let i3 = (latitude + 1) * (4 * quality + 1) + longitude + 1;
            indices.extend_from_slice(&[
                i0 as u32, i1 as u32, i2 as u32, i2 as u32, i1 as u32, i3 as u32,
            ]);
        }
    }

    (vertices, indices)
}

struct Practice10 {
    depth_buffer: Option<wgpu::Texture>,
    depth_buffer_view: Option<wgpu::TextureView>,
    camera_uniform_buffer: wgpu::Buffer,
    albedo_texture: wgpu::Texture,
    bind_group: wgpu::BindGroup,
    pipeline: wgpu::RenderPipeline,
    vertex_buffer: wgpu::Buffer,
    index_buffer: wgpu::Buffer,
    index_count: u32,
    last_frame_start: Instant,
    time: f32,
    camera_distance: f32,
    camera_rotation: f32,
    camera_azimuth: f32,
}

impl WgpuApp for Practice10 {
    fn new(app: &WgpuState) -> Self {
        let shader = load_shader_module(&app.device, Path::new(PROJECT_ROOT).join("shader.wgsl"))
            .expect("failed to load shader");

        let bind_group_layout =
            app.device
                .create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
                    label: None,
                    entries: &[
                        wgpu::BindGroupLayoutEntry {
                            binding: 0,
                            visibility: wgpu::ShaderStages::VERTEX | wgpu::ShaderStages::FRAGMENT,
                            ty: wgpu::BindingType::Buffer {
                                ty: wgpu::BufferBindingType::Uniform,
                                has_dynamic_offset: false,
                                min_binding_size: None,
                            },
                            count: None,
                        },
                        wgpu::BindGroupLayoutEntry {
                            binding: 1,
                            visibility: wgpu::ShaderStages::FRAGMENT,
                            ty: wgpu::BindingType::Sampler(wgpu::SamplerBindingType::Filtering),
                            count: None,
                        },
                        wgpu::BindGroupLayoutEntry {
                            binding: 2,
                            visibility: wgpu::ShaderStages::FRAGMENT,
                            ty: wgpu::BindingType::Texture {
                                sample_type: wgpu::TextureSampleType::Float { filterable: true },
                                view_dimension: wgpu::TextureViewDimension::D2,
                                multisampled: false,
                            },
                            count: None,
                        },
                    ],
                });

        let camera_uniform_buffer = app.device.create_buffer(&wgpu::BufferDescriptor {
            label: None,
            size: mem::size_of::<CameraUniforms>() as wgpu::BufferAddress,
            usage: wgpu::BufferUsages::COPY_DST | wgpu::BufferUsages::UNIFORM,
            mapped_at_creation: false,
        });

        let albedo_texture = load_texture(
            &app.device,
            &app.queue,
            Path::new(PROJECT_ROOT).join("textures/albedo.jpg"),
            wgpu::TextureFormat::Rgba8Unorm,
        );
        let albedo_texture_view =
            albedo_texture.create_view(&wgpu::TextureViewDescriptor::default());
        let linear_sampler = app.device.create_sampler(&wgpu::SamplerDescriptor {
            label: None,
            address_mode_u: wgpu::AddressMode::Repeat,
            address_mode_v: wgpu::AddressMode::ClampToEdge,
            address_mode_w: wgpu::AddressMode::ClampToEdge,
            mag_filter: wgpu::FilterMode::Linear,
            min_filter: wgpu::FilterMode::Linear,
            mipmap_filter: wgpu::MipmapFilterMode::Nearest,
            ..Default::default()
        });

        let bind_group = app.device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: None,
            layout: &bind_group_layout,
            entries: &[
                wgpu::BindGroupEntry {
                    binding: 0,
                    resource: camera_uniform_buffer.as_entire_binding(),
                },
                wgpu::BindGroupEntry {
                    binding: 1,
                    resource: wgpu::BindingResource::Sampler(&linear_sampler),
                },
                wgpu::BindGroupEntry {
                    binding: 2,
                    resource: wgpu::BindingResource::TextureView(&albedo_texture_view),
                },
            ],
        });

        let pipeline_layout = app
            .device
            .create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
                label: None,
                bind_group_layouts: &[Some(&bind_group_layout)],
                ..Default::default()
            });
        let pipeline = app
            .device
            .create_render_pipeline(&wgpu::RenderPipelineDescriptor {
                label: None,
                layout: Some(&pipeline_layout),
                vertex: wgpu::VertexState {
                    module: &shader,
                    entry_point: Some("vertexMain"),
                    buffers: &[Some(wgpu::VertexBufferLayout {
                        array_stride: mem::size_of::<Vertex>() as wgpu::BufferAddress,
                        step_mode: wgpu::VertexStepMode::Vertex,
                        attributes: &[
                            wgpu::VertexAttribute {
                                offset: mem::offset_of!(Vertex, position) as wgpu::BufferAddress,
                                shader_location: 0,
                                format: wgpu::VertexFormat::Float32x3,
                            },
                            wgpu::VertexAttribute {
                                offset: mem::offset_of!(Vertex, normal) as wgpu::BufferAddress,
                                shader_location: 1,
                                format: wgpu::VertexFormat::Float32x3,
                            },
                            wgpu::VertexAttribute {
                                offset: mem::offset_of!(Vertex, texcoord) as wgpu::BufferAddress,
                                shader_location: 2,
                                format: wgpu::VertexFormat::Float32x2,
                            },
                            wgpu::VertexAttribute {
                                offset: mem::offset_of!(Vertex, tangent) as wgpu::BufferAddress,
                                shader_location: 3,
                                format: wgpu::VertexFormat::Float32x3,
                            },
                        ],
                    })],
                    compilation_options: Default::default(),
                },
                fragment: Some(wgpu::FragmentState {
                    module: &shader,
                    entry_point: Some("fragmentMain"),
                    targets: &[Some(wgpu::ColorTargetState {
                        format: app.surface_format(),
                        blend: None,
                        write_mask: wgpu::ColorWrites::ALL,
                    })],
                    compilation_options: Default::default(),
                }),
                primitive: wgpu::PrimitiveState {
                    topology: wgpu::PrimitiveTopology::TriangleList,
                    cull_mode: Some(wgpu::Face::Back),
                    ..Default::default()
                },
                depth_stencil: Some(wgpu::DepthStencilState {
                    format: wgpu::TextureFormat::Depth24Plus,
                    depth_write_enabled: Some(true),
                    depth_compare: Some(wgpu::CompareFunction::Less),
                    stencil: Default::default(),
                    bias: Default::default(),
                }),
                multisample: Default::default(),
                multiview_mask: None,
                cache: None,
            });

        let (vertices, indices) = generate_sphere(1.0, 16);
        let vertex_buffer = load_buffer(&app.device, &vertices, wgpu::BufferUsages::VERTEX);
        let index_buffer = load_buffer(&app.device, &indices, wgpu::BufferUsages::INDEX);

        Self {
            depth_buffer: None,
            depth_buffer_view: None,
            camera_uniform_buffer,
            albedo_texture,
            bind_group,
            pipeline,
            vertex_buffer,
            index_buffer,
            index_count: indices.len() as u32,
            last_frame_start: Instant::now(),
            time: 0.0,
            camera_distance: 3.0,
            camera_rotation: 0.0,
            camera_azimuth: PI / 12.0,
        }
    }

    fn redraw(&mut self, app: &mut WgpuState) {
        let Some(surface_texture) = app.begin_frame() else {
            return;
        };

        if self.depth_buffer.as_ref().is_none_or(|depth_buffer| {
            depth_buffer.width() != app.width() || depth_buffer.height() != app.height()
        }) {
            let depth_buffer = app.device.create_texture(&wgpu::TextureDescriptor {
                label: None,
                size: wgpu::Extent3d {
                    width: app.width(),
                    height: app.height(),
                    depth_or_array_layers: 1,
                },
                mip_level_count: 1,
                sample_count: 1,
                dimension: wgpu::TextureDimension::D2,
                format: wgpu::TextureFormat::Depth24Plus,
                usage: wgpu::TextureUsages::RENDER_ATTACHMENT
                    | wgpu::TextureUsages::TRANSIENT_ATTACHMENT,
                view_formats: &[],
            });
            let depth_buffer_view =
                depth_buffer.create_view(&wgpu::TextureViewDescriptor::default());
            self.depth_buffer = Some(depth_buffer);
            self.depth_buffer_view = Some(depth_buffer_view);
        }

        let now = Instant::now();
        let dt = (now - self.last_frame_start).as_secs_f32();
        self.time += dt;
        self.last_frame_start = now;

        if app.keydown.contains(&KeyCode::ArrowLeft) {
            self.camera_rotation += 3.0 * dt;
        }
        if app.keydown.contains(&KeyCode::ArrowRight) {
            self.camera_rotation -= 3.0 * dt;
        }
        if app.keydown.contains(&KeyCode::ArrowUp) {
            self.camera_distance += 3.0 * dt;
        }
        if app.keydown.contains(&KeyCode::ArrowDown) {
            self.camera_distance -= 3.0 * dt;
        }

        let view = Mat4::from_translation(Vec3::new(0.0, 0.0, -self.camera_distance))
            * Mat4::from_rotation_x(self.camera_azimuth)
            * Mat4::from_rotation_y(-self.camera_rotation);
        let projection = Mat4::perspective_rh(
            PI / 3.0,
            app.width() as f32 / app.height() as f32,
            0.01,
            100.0,
        );
        let camera_uniforms = CameraUniforms {
            view_projection: projection * view,
            position: (view.inverse() * Vec4::new(0.0, 0.0, 0.0, 1.0)).truncate(),
            padding: 0.0,
        };
        app.queue.write_buffer(
            &self.camera_uniform_buffer,
            0,
            bytemuck::bytes_of(&camera_uniforms),
        );

        let target_view = surface_texture
            .texture
            .create_view(&wgpu::TextureViewDescriptor::default());
        let mut encoder = app.device.create_command_encoder(&Default::default());
        {
            let mut render_pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: &target_view,
                    depth_slice: None,
                    resolve_target: None,
                    ops: wgpu::Operations {
                        load: wgpu::LoadOp::Clear(wgpu::Color {
                            r: 0.6,
                            g: 0.8,
                            b: 1.0,
                            a: 1.0,
                        }),
                        store: wgpu::StoreOp::Store,
                    },
                })],
                depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment {
                    view: self.depth_buffer_view.as_ref().unwrap(),
                    depth_ops: Some(wgpu::Operations {
                        load: wgpu::LoadOp::Clear(1.0),
                        store: wgpu::StoreOp::Discard,
                    }),
                    stencil_ops: None,
                }),
                ..Default::default()
            });
            render_pass.set_pipeline(&self.pipeline);
            render_pass.set_bind_group(0, &self.bind_group, &[]);
            render_pass.set_vertex_buffer(0, self.vertex_buffer.slice(..));
            render_pass.set_index_buffer(self.index_buffer.slice(..), wgpu::IndexFormat::Uint32);
            render_pass.draw_indexed(0..self.index_count, 0, 0..1);
        }

        app.queue.submit(std::iter::once(encoder.finish()));
        app.queue.present(surface_texture);
    }
}

fn main() {
    run::<Practice10>(AppConfig {
        title: "Practice10",
        width: 1280,
        height: 720,
        srgb: false,
    });
}
