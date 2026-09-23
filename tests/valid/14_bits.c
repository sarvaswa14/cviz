int popcount(int x) {
    int c = 0;
    while (x) {
        c += x & 1;
        x = x >> 1;
    }
    return c;
}

int main(void) {
    int x = 0x5A;
    printf("x = %d, hex %x\n", x, x);
    printf("x << 3 = %d, x >> 2 = %d\n", x << 3, x >> 2);
    printf("x & 15 = %d, x | 1 = %d, x ^ 255 = %d, ~x = %d\n", x & 15, x | 1, x ^ 255, ~x);
    printf("popcount(%d) = %d\n", x, popcount(x));
    printf("-7 / 2 = %d, -7 %% 2 = %d, 7 / -2 = %d\n", -7 / 2, -7 % 2, 7 / -2);
    printf("char '%c' is %d\n", 'A' + 2, 'A' + 2);
    return popcount(x);
}
