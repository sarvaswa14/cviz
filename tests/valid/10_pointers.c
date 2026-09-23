int sum(int *p, int n) {
    int total = 0;
    int *end = p + n;
    while (p < end) {
        total += *p;
        p++;
    }
    return total;
}

void fill(int *p, int n, int start) {
    int i;
    for (i = 0; i < n; i++) *(p + i) = start + i * i;
}

int main(void) {
    int v[8];
    int *mid;
    fill(v, 8, 3);
    printf("sum = %d\n", sum(v, 8));
    mid = v + 4;
    printf("mid[0] = %d, mid[-1] = %d\n", mid[0], mid[-1]);
    printf("distance = %d\n", (int)(mid - v));
    printf("sizeof(v) = %d\n", (int)sizeof(v));
    return 0;
}
