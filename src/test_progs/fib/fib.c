#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]){
    if (argc != 2){
        printf("wrong usage, the right way to use is\n%s number\n",argv[0]);
        return 0;
    }
    int x=atoi(argv[1]);
    int fib,fib_menos_1=1,fib_menos_2=0,aux;
    if (x==0){
        printf("%d\n",fib_menos_2);
    }else if(x==1){
        printf("%d\n",fib_menos_1);
    }else{

        for(int i=2;i<=x;i++){
            fib = fib_menos_1+fib_menos_2;
            fib_menos_2 = fib_menos_1;
            fib_menos_1 = fib;
        }
    }
    printf("%d\n",fib);
}
