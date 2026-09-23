/* Values arrive as parameters, so constant folding cannot remove them and
   the redundancy passes are the ones that act. */
int mix(int a, int b) {
    int c, d, x, y;

    c = a + b;        /* a + b computed here            */
    a = a + 10;       /* one operand then changes       */
    d = a + b;        /* recomputed, must not be reused */

    x = b;            /* a copy                         */
    y = x;
    b = b * 2;        /* its source then changes        */

    printf("c = %d, d = %d, x = %d, y = %d, b = %d\n", c, d, x, y, b);
    return c + d + y;
}

int main(void) {
    int t = 0;
    t += mix(3, 4);
    t += mix(20, 5);
    printf("total = %d\n", t);
    return t % 256;
}
