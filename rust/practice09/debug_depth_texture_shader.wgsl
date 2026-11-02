@group(0) @binding(1) var shadowMap: texture_depth_2d;
@group(0) @binding(2) var shadowMapSampler: sampler;

struct VertexOut {
	@builtin(position) clip_position: vec4f,
	@location(0) texcoord: vec2f,
}

const VERTICES = array<vec2f, 6>(
	vec2f(-1.0, -1.0),
	vec2f( 1.0, -1.0),
	vec2f(-1.0,  1.0),
	vec2f(-1.0,  1.0),
	vec2f( 1.0, -1.0),
	vec2f( 1.0,  1.0),
);

struct Immediates {
	center: vec2f,
	extent: vec2f,
}

var<immediate> immediates: Immediates;

@vertex
fn vertexMain(@builtin(vertex_index) vertex_id: u32) -> VertexOut {
	let vertex = VERTICES[vertex_id];
	return VertexOut(
		vec4f(immediates.center + vertex * immediates.extent, 0.0, 1.0),
		vec2f(0.5) + vec2f(0.5, -0.5) * vertex
	);
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
	return vec4f(textureSample(shadowMap, shadowMapSampler, in.texcoord));
}
