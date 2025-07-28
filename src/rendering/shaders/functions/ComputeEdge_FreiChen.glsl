// Frei-Chen edge detection convolution filters:
const mat3 Filter_FC[9] = mat3[](
  1.0 / (2.0 * sqrt(2.0)) * mat3(1.0, sqrt(2.0), 1.0, 0.0, 0.0, 0.0, -1.0, -sqrt(2.0), -1.0),
  1.0 / (2.0 * sqrt(2.0)) * mat3(1.0, 0.0, -1.0, sqrt(2.0), 0.0, -sqrt(2.0), 1.0, 0.0, -1.0),
  1.0 / (2.0 * sqrt(2.0)) * mat3(0.0, -1.0, sqrt(2.0), 1.0, 0.0, -1.0, -sqrt(2.0), 1.0, 0.0),
  1.0 / (2.0 * sqrt(2.0)) * mat3(sqrt(2.0), -1.0, 0.0, -1.0, 0.0, 1.0, 0.0, 1.0, -sqrt(2.0)),
  1.0 / 2.0 * mat3(0.0, 1.0, 0.0, -1.0, 0.0, -1.0, 0.0, 1.0, 0.0),
  1.0 / 2.0 * mat3(-1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, -1.0),
  1.0 / 6.0 * mat3(1.0, -2.0, 1.0, -2.0, 4.0, -2.0, 1.0, -2.0, 1.0),
  1.0 / 6.0 * mat3(-2.0, 1.0, -2.0, 1.0, 4.0, 1.0, -2.0, 1.0, -2.0),
  1.0 / 3.0 * mat3(1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0)
);

float computeEdge(mat3 V, float edgeMagnitude)
{
  float C_FC[9];
  for (int i = 0; i <= 8; ++i) {
    C_FC[i] = dot(Filter_FC[i][0], V[0]) + dot(Filter_FC[i][1], V[1]) + dot(Filter_FC[i][2], V[2]);
    C_FC[i] *= C_FC[i];
  }

  float M_FC = (C_FC[0] + C_FC[1]) + (C_FC[2] + C_FC[3]);
  float S_FC = (C_FC[4] + C_FC[5]) + (C_FC[6] + C_FC[7]) + (C_FC[8] + M_FC);

  return sqrt(M_FC / S_FC) / max(edgeMagnitude, 0.01);
}
