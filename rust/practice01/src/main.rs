use wgpu_app::{run, AppConfig, WgpuApp, WgpuState};

const PROJECT_ROOT: &str = env!("CARGO_MANIFEST_DIR");

struct Practice01 {}

impl WgpuApp for Practice01 {
    fn new(app: &WgpuState) -> Self {
        Self{}
    }

    fn redraw(&mut self, app: &mut WgpuState) {
        let Some(surface_texture) = app.begin_frame() else {
            return;
        };

        let target_view = surface_texture
            .texture
            .create_view(&wgpu::TextureViewDescriptor::default());

        // Frame rendering code goes here

        app.queue.present(surface_texture);
    }
}

fn main() {
    run::<Practice01>(AppConfig {
        title: "Practice01",
        width: 1280,
        height: 720,
        srgb: false,
    });
}
