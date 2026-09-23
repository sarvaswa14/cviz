int scale;

int compute(int x) {
    int a = 4 * 8 + 2;
    int b = a - 34;
    int c = x * 1 + b;
    int d = (x + a) * (x + a);
    int unused = x * 100 + 7;
    if (b) {
        c = c + 1000;
    }
    return c + d - 0;
}

int main(void) {
    int i, total = 0;
    int k = 3 * 3;
    scale = k - 9 + 2;
    for (i = 0; i < 5; i++) {
        total = total + compute(i) * scale;
    }
    printf("total = %d\n", total);
    printf("k = %d, scale = %d\n", k, scale);
    return 0;
}
