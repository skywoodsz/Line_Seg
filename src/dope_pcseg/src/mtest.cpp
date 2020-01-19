#include <cstdlib>
#include <iostream>
#include <queue>
  
using namespace std;
  
int main()
{
    int e,n,m;
    queue<int> q1;
    for(int i=0;i<10;i++)
       q1.push(i);
    if(!q1.empty())
    cout<<"dui lie  bu kong\n";
    n=q1.size();
    cout<<n<<endl;
    m=q1.back();
    cout<<m<<endl;
    for(int j=0;j<n;j++)
    {
       e=q1.front();
       cout<<e<<" ";
       q1.pop();
    }
    cout<<endl;
    if(q1.empty())
    cout<<"dui lie  bu kong\n";
    return 0;
}