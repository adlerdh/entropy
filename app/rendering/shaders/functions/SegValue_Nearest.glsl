/// Default nearest-neighbor lookup.
uint getSegValue(vec3 texOffset, out float opacity)
{
  opacity = 1.0;
  return safeSegLookup(sampleTexCoord(fs_in.v_texCoord, fs_in.v_worldPos) + texOffset);
}
