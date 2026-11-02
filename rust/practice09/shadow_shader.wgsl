struct VertexIn {
    @location(0) position: vec3f,
}

struct VertexOut {
    @builtin(position) clip_position: vec4f,
}

struct Immediates {
	shadowProjection: mat4x4f,
}

var<immediate> immediates: Immediates;

@vertex
fn vertexMain(in: VertexIn) -> VertexOut {
	return VertexOut(immediates.shadowProjection * vec4f(in.position, 1.0));
}

