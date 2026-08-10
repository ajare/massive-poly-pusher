@@Version

void main()
{
    // Deliberately unlit: the light marker neither receives shadows nor
    // depends on the legacy/PBR lighting contracts.
    @Out(vec4 COLOUR) = vec4(1.0, 0.82, 0.05, 1.0);
}
