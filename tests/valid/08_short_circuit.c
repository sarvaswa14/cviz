int calls;

int noisy(int v) {
    calls = calls + 1;
    return v;
}

int main(void) {
    int r;
    calls = 0;
    r = noisy(0) && noisy(1);
    printf("0 && 1 = %d, calls = %d\n", r, calls);
    calls = 0;
    r = noisy(1) || noisy(0);
    printf("1 || 0 = %d, calls = %d\n", r, calls);
    calls = 0;
    r = noisy(1) && noisy(2) && noisy(0);
    printf("1 && 2 && 0 = %d, calls = %d\n", r, calls);
    r = !noisy(0) ? 7 : 9;
    printf("ternary = %d\n", r);
    return calls;
}
