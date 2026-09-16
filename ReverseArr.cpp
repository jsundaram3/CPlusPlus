#include <iostream>

void swap(int *p, int l, int r)
{
    int temp = *(p+r);
    *(p+r) = *(p+l);
    *(p+l) = temp;
}
void reverse (int *p, int l, int r)
{
    if(l >= r) return;
    swap(p, l, r);
    reverse(p, l+1, r-1);
}
int main()
{
    int arr[10] = {2, 3, 4, 5, 6, 7, 9 ,12, 23, 42};
    reverse (arr, 0, 9);
    for (auto x : arr)
        std::cout << x << ", " ;
}