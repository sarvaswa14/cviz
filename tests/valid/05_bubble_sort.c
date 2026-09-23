void sort(int *a, int n) {
    int i, j, t;
    for (i = 0; i < n - 1; i++) {
        for (j = 0; j < n - 1 - i; j++) {
            if (a[j] > a[j + 1]) {
                t = a[j];
                a[j] = a[j + 1];
                a[j + 1] = t;
            }
        }
    }
}

int main(void) {
    int data[10];
    int i, seed = 7;
    for (i = 0; i < 10; i++) {
        seed = (seed * 31 + 11) % 97;
        data[i] = seed;
    }
    sort(data, 10);
    for (i = 0; i < 10; i++) printf("%d ", data[i]);
    printf("\n");
    return data[0];
}
