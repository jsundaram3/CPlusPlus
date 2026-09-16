#include <iostream>

int maximum (int *p, int index)
{
    int max;
    if(index < 0) return max;

    maximum (p, index -1);
    if( *(p+index) > max)
    {
        max = *(p+index);
    }
    return max;

}
int main()
{
    int arr[10] = {12, 34, 12, 542, 45, 87, 5, 2, 1, 8};
    std::cout << maximum(&arr, 9);
    return 0;
}