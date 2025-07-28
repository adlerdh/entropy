// Sobel edge detection convolution filters:
// Scharr version (see https://en.wikipedia.org/wiki/Sobel_operator)
const float A = 1.0;
const float B = 2.0;

const mat3 Filter_Sobel[2] = mat3[](
  mat3(A,   B,  A,
       0,   0,  0,
       -A, -B, -A),
  mat3(A, 0, -A,
       B, 0, -B,
       A, 0, -A)
);

const float SobelFactor = 1.0 / (2.0 * A + B);

// Returns gradient magnitude of Sobel filter
float computeEdge(mat3 V, float edgeMagnitude)
{
  // Convolutions for all masks:
  float C_Sobel[2];
  for (int i = 0; i <= 1; ++i) {
    C_Sobel[i] = dot(Filter_Sobel[i][0], V[0]) + dot(Filter_Sobel[i][1], V[1]) + dot(Filter_Sobel[i][2], V[2]);
    C_Sobel[i] *= C_Sobel[i];
  }

  return SobelFactor * sqrt(C_Sobel[0] + C_Sobel[1]) / max(edgeMagnitude, 0.01);
}
