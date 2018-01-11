This takes a set of glsl shader files and converts it to the follow json (based on gltf 1.0):

{ 
    [id: string]: Shader; // id = Shader.name + ("vs" | "fs")
}

where Shader =
{
    name: string;
    type: 35632 | 35633; // fs = 35632, vs = 35633
    attributes: string[];
    uniforms: string[];
    source: string;
}