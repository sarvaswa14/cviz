int a;
int *p;
int arr[10];
int *ptrs[5];
int (*aptr)[5];
int matrix[3][4];
char **argv;

int add(int x, int y) {
    return x + y;
}

int fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}
