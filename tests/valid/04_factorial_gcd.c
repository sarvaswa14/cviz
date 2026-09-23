int factorial(int n) {
    int r = 1;
    while (n > 1) {
        r = r * n;
        n = n - 1;
    }
    return r;
}

int gcd(int a, int b) {
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a;
}

int main(void) {
    int i;
    for (i = 0; i <= 10; i++) printf("%d! = %d\n", i, factorial(i));
    printf("gcd(1071, 462) = %d\n", gcd(1071, 462));
    printf("gcd(270, 192) = %d\n", gcd(270, 192));
    return 0;
}
