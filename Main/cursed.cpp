#include <stdio.h>
#include <curses.h>
using namespace std;
int main()
{
    char ch=0;
    printf("Press Q to quit\n");
    do
    {
        ch = getch();

        switch(ch)
        {
            case 'W':
            case 'w':
                printf("W was pressed \n");
                break;
            case 'A':
            case 'a':
                printf("A was pressed \n");
                break;
            case 's':
            case 'S':
                printf("S was pressed \n");
                break;
            case 'D':
            case 'd':
                printf("D was pressed \n");
                break;

        }

    }while (ch != 'Q' && ch!='q');
}
