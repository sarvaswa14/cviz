int moves;
int stack[16];
int top = 0;

void hanoi(int n, int from, int to, int via) {
    if (n == 0) return;
    hanoi(n - 1, from, via, to);
    moves++;
    hanoi(n - 1, via, to, from);
}

void push(int v) {
    stack[top] = v;
    top++;
}

int pop(void) {
    top--;
    return stack[top];
}

int main(void) {
    int i;
    hanoi(6, 1, 3, 2);
    printf("hanoi(6) needs %d moves\n", moves);
    for (i = 1; i <= 5; i++) push(i * i);
    while (top > 0) printf("%d ", pop());
    printf("\n");
    return moves % 256;
}
