int main(void) {
    int composite[101];
    int i, j, count = 0;
    for (i = 0; i <= 100; i++) composite[i] = 0;
    for (i = 2; i * i <= 100; i++) {
        if (composite[i]) continue;
        for (j = i * i; j <= 100; j += i) composite[j] = 1;
    }
    for (i = 2; i <= 100; i++) {
        if (composite[i]) continue;
        printf("%d ", i);
        count++;
        if (count == 20) break;
    }
    printf("\n%d primes printed\n", count);
    return count;
}
