int classify(int x) {
    int score = 0;
    switch (x) {
        case 0:
            score = 100;
            break;
        case 1:
        case 2:
            score += 10;
        case 3:
            score += 20;
            break;
        case -1:
            score = -5;
            break;
        default:
            score = x * 2;
    }
    return score;
}

int main(void) {
    int i;
    for (i = -2; i <= 5; i++) printf("classify(%d) = %d\n", i, classify(i));
    return 0;
}
