int a[3][3];
int b[3][3];
int c[3][3];

int main(void) {
    int i, j, k;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            a[i][j] = i + j;
            b[i][j] = i * 3 + j + 1;
        }
    }
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            c[i][j] = 0;
            for (k = 0; k < 3; k++) c[i][j] += a[i][k] * b[k][j];
        }
    }
    for (i = 0; i < 3; i++) {
        printf("%4d %4d %4d\n", c[i][0], c[i][1], c[i][2]);
    }
    return c[2][2] % 256;
}
