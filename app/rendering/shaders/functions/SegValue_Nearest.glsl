/// Default nearest-neighbor lookup.
uint getSegValue(vec3 texOffset, out float opacity)
{
  opacity = 1.0;
  return uintTextureLookup(u_segTex, fs_in.v_texCoord + texOffset);
}
