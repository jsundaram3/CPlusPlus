#include <iostream>

int maximum (int *p, int index)
{
    int max =0;
    if(index < 0) return max;

    max = maximum (p, index -1);
    if( *(p+index) > max)
    {
        max = *(p+index);
    }
    return max;

}

void maximum (int *p, int index, int& max)
{
    if(index < 0) return;
    if(*(p + index) > max) // p points to arr[0] , so add index to get the number from the index location
        max = *(p+index) ;
    maximum (p, index -1, max);
}
int main()
{
    int size = 10;
    int arr[10] = {12, 34, 12, 542, 45, 87, 5, 2, 1, 8};
    std::cout << maximum(arr, size-1) << std::endl;
    int max = 0;
    maximum (arr, size-1, max);
    std:: cout << "Maximum :" << max << std::endl;
    return 0;
}
