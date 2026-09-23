int steps(int n) {
    int count = 0;
    do {
        n = (n % 2 == 0) ? n / 2 : 3 * n + 1;
        count++;
    } while (n != 1);
    return count;
}

int main(void) {
    int n, best = 0, arg = 0;
    for (n = 2; n < 60; n++) {
        int s = steps(n);
        if (s > best) {
            best = s;
            arg = n;
        }
    }
    printf("longest chain below 60 starts at %d with %d steps\n", arg, best);
    n = 100;
    n -= 1;
    n *= 2;
    n /= 3;
    n %= 50;
    printf("compound = %d\n", n);
    return arg;
}
