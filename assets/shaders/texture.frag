// The #version and precision qualifiers are injected by createShaderProgram

in vec2 vTexCoord;

uniform sampler2D ourTexture;

out vec4 fragColor;

void main()
{
    fragColor = texture(ourTexture, vTexCoord);
}
