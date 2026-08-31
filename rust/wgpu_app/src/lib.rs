use std::sync::Arc;

use wgpu::CurrentSurfaceTexture;
use winit::application::ApplicationHandler;
use winit::dpi::{LogicalSize, PhysicalSize};
use winit::event::WindowEvent;
use winit::event_loop::{ActiveEventLoop, EventLoop};
use winit::window::{Window, WindowAttributes, WindowId};

#[derive(Clone, Copy)]
pub struct AppConfig {
    pub title: &'static str,
    pub width: u32,
    pub height: u32,
    pub srgb: bool,
}

pub struct WgpuState {
    pub window: Arc<Window>,
    surface: wgpu::Surface<'static>,
    pub device: wgpu::Device,
    pub queue: wgpu::Queue,
    config: wgpu::SurfaceConfiguration,
}

impl WgpuState {
    pub fn new(window: Arc<Window>, srgb: bool) -> Self {
        pollster::block_on(Self::init_async(window, srgb))
    }

    pub fn new_from_event_loop(event_loop: &ActiveEventLoop, config: AppConfig) -> Self {
        let window = Arc::new(
            event_loop
                .create_window(
                    WindowAttributes::default()
                        .with_title(config.title)
                        .with_inner_size(LogicalSize::new(config.width, config.height)),
                )
                .expect("failed to create window"),
        );

        Self::new(window, config.srgb)
    }

    async fn init_async(window: Arc<Window>, srgb: bool) -> Self {
        let size = window.inner_size();

        let instance = wgpu::Instance::new(wgpu::InstanceDescriptor {
            backends: wgpu::Backends::PRIMARY,
            ..wgpu::InstanceDescriptor::new_without_display_handle()
        });

        let surface = instance
            .create_surface(window.clone())
            .expect("failed to create surface");

        let adapter = instance
            .request_adapter(&wgpu::RequestAdapterOptions {
                power_preference: wgpu::PowerPreference::default(),
                compatible_surface: Some(&surface),
                force_fallback_adapter: false,
                apply_limit_buckets: false,
            })
            .await
            .expect("no compatible GPU adapter");

        let (device, queue) = adapter
            .request_device(&wgpu::DeviceDescriptor {
                required_features: wgpu::Features::IMMEDIATES,
                required_limits: wgpu::Limits {
                    max_immediate_size: 128,
                    ..Default::default()
                },
                memory_hints: wgpu::MemoryHints::default(),
                ..Default::default()
            })
            .await
            .expect("failed to create device");

        let caps = surface.get_capabilities(&adapter);
        let format = caps
            .formats
            .iter()
            .copied()
            .find(|format| format.is_srgb() == srgb)
            .expect(if srgb {
                "surface has no sRGB format"
            } else {
                "surface has no linear format"
            });

        let config = wgpu::SurfaceConfiguration {
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
            format,
            color_space: wgpu::SurfaceColorSpace::Auto,
            width: size.width.max(1),
            height: size.height.max(1),
            present_mode: wgpu::PresentMode::Fifo,
            alpha_mode: caps.alpha_modes[0],
            view_formats: vec![],
            desired_maximum_frame_latency: 2,
        };

        surface.configure(&device, &config);

        Self {
            window,
            surface,
            device,
            queue,
            config,
        }
    }

    pub fn surface_format(&self) -> wgpu::TextureFormat {
        self.config.format
    }

    pub fn size(&self) -> PhysicalSize<u32> {
        PhysicalSize::new(self.config.width, self.config.height)
    }

    pub fn resize(&mut self, new_size: PhysicalSize<u32>) {
        if new_size.width == 0 || new_size.height == 0 {
            return;
        }

        self.config.width = new_size.width;
        self.config.height = new_size.height;
        self.surface.configure(&self.device, &self.config);
    }

    pub fn begin_frame(&mut self) -> Option<wgpu::SurfaceTexture> {
        match self.surface.get_current_texture() {
            CurrentSurfaceTexture::Success(texture) | CurrentSurfaceTexture::Suboptimal(texture) => {
                Some(texture)
            }
            CurrentSurfaceTexture::Outdated | CurrentSurfaceTexture::Lost => {
                self.resize(self.window.inner_size());
                None
            }
            CurrentSurfaceTexture::Timeout
            | CurrentSurfaceTexture::Occluded
            | CurrentSurfaceTexture::Validation => None,
        }
    }
}

pub trait WgpuApp {
    fn new(gpu: &WgpuState) -> Self;
    fn redraw(&mut self, gpu: &mut WgpuState);
}

pub fn run<P: WgpuApp>(config: AppConfig) {
    struct App<P> {
        config: AppConfig,
        state: Option<(WgpuState, P)>,
    }

    impl<P: WgpuApp> ApplicationHandler for App<P> {
        fn resumed(&mut self, event_loop: &ActiveEventLoop) {
            let (gpu, _) = self.state.get_or_insert_with(|| {
                let gpu = WgpuState::new_from_event_loop(event_loop, self.config);
                let practice = P::new(&gpu);
                (gpu, practice)
            });
            gpu.window.request_redraw();
        }

        fn window_event(
            &mut self,
            event_loop: &ActiveEventLoop,
            _window_id: WindowId,
            event: WindowEvent,
        ) {
            let Some((gpu, practice)) = &mut self.state else {
                return;
            };

            match event {
                WindowEvent::CloseRequested => event_loop.exit(),
                WindowEvent::Resized(size) => {
                    gpu.resize(size);
                    gpu.window.request_redraw();
                }
                WindowEvent::RedrawRequested => {
                    practice.redraw(gpu);
                    gpu.window.request_redraw();
                }
                _ => {}
            }
        }
    }

    let event_loop = EventLoop::new().expect("failed to create event loop");
    event_loop
        .run_app(&mut App::<P> {
            config,
            state: None,
        })
        .expect("event loop failed");
}
