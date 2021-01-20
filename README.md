
# MALLOC_LAB
동적 메모리 할당 구현   
_이 글은 <Computer Systems A Programmer's Perspective>, Randal E. Bryant의 책을 기준으로 작성하였습니다._

>**구현 이슈**
>* malloc을 위한 가용블럭을 어떻게 관리할 것인가?
>* 할당 블럭에서 가용 블럭으로 바뀐 공간을 어떻게 효율적으로 재사용 할 것인가?

## Implicit (묵시적 가용리스트)
<img src="https://user-images.githubusercontent.com/61036124/105174781-68531800-5b66-11eb-921c-b6b3200f1052.jpg" width="750px" title="px(픽셀) 크기 설정" alt="RubberDuck"></img><br/>
명시적 리스트는 사진과 같은 모양으로 데이터를 힙에 저장한다.  
*header와 footer**
블럭마다 맨 앞, 맨 뒤에 **header**와 **footer**가 존재하고 4byte + 4byte의 이 공간은 블럭의 크기와 할당 유무를 알려주는 역할을 한다.   

할당된 블럭을 확대해보면 이렇게 구성되어있다.   
<img src="https://user-images.githubusercontent.com/61036124/105175400-33939080-5b67-11eb-99b2-8b23c7163e2b.jpg" width="750px" title="px(픽셀) 크기 설정" alt="RubberDuck"></img><br/>
header에는 header와 footer의 크기를 포함한 블럭의 전체 크기 정보와 이 블럭이 할당되어있는지에 대한 정보를 저장하고 있다.   

>여기서 한가지 짚고 넘어가야할 부분은 **우리가 다루는 데이터의 크기를 항상 8의 배수 byte의 크기여야한다**는 점이다. 책은 이 부분에 대해 32비트와 64비트 모두 상관 없이 돌아가는 코드를 만들기 위해서 기본 데이터의 크기를 8byte를 기준으로 한다고 설명한다.   

>그림에서는 최소 데이터 크기인 8byte를 기준으로 2칸만 할당했지만 실제로는 8byte가 넘는 값이 들어갈 수 있다는 것을 유의해야한다.   

데이터는 항상 8의 배수이기 떄문에 2진법으로 나타냈을 때 마지막 3개의 자리수가 항상 000으로 끝난다. 따라서 이 부분의 제일 마지막 자리를 할당 유무를 저장해주는 용도로 사용할 수 있다.   

<img src="https://user-images.githubusercontent.com/61036124/105176373-8752a980-5b68-11eb-9a01-701b6bb85637.jpg" width="750px" title="px(픽셀) 크기 설정" alt="RubberDuck"></img><br/>
마지막 비트를 0으로 두면 해당 블럭을 가용 블럭이라는 정보를 저장할 수 있다.   

<img src="https://user-images.githubusercontent.com/61036124/105176749-ff20d400-5b68-11eb-9a09-91a33ea069b7.jpg" width="420px" title="px(픽셀) 크기 설정" alt="RubberDuck"></img><br/>
프롤로그 헤더에 저장된 정보이다. 프롤로그 블럭은 할당가능한 공간의 맨 앞을 가리키기 때문에 header와 footer만 가지고 있다. 따라서 프롤로그 블럭의 총 크기는 8byte이고 항상 할당되어있는 블럭이기 때문에 size : 8, allocation : 1 이라는 정보를 갖고 있다.   
   
에필로그 헤더는 할당 가능한 메모리 공간의 제일 마지막을 가리킨다. 이 블럭은 size : 0을 가지고 있게 설정해주어서 제일 마지막에 도달했을 때 새로운 메모리 공간을 힙으로부터 가져와야한다는 것을 알려주는 역할을 한다.


### first_fit
적절한 가용 공간을 찾을 때 맨 앞에서부터 다음 블럭으로 이동하면서 하나씩 확인하는 방법이다.   
<img src="https://user-images.githubusercontent.com/61036124/105178841-e5cd5700-5b6b-11eb-8da6-d81b35424cf0.jpg" width="750px" title="px(픽셀) 크기 설정" alt="RubberDuck"></img><br/>
맨 앞(header_listp)부터 차례대로  1. 가용블럭인지 (= allocation이 0인지),  2. 블럭의 size가 할당해야하는 size보다 큰지를 확인하면서 조건에 충족하는 블럭에 새롭게 받아온 데이터를 할당해준다.   


### next_fit
적절한 가용 블럭을 찾을 때 이전에 찾았던 공간 부터 다음 블럭으로 이동하면서 하나씩 확인하는 방법인다. 에필로그 블럭까지 도달한 후에 처음으로 돌아와서 탐색을 시작한다.   
<img src="https://user-images.githubusercontent.com/61036124/105180044-680a4b00-5b6d-11eb-85aa-822741e0a8cd.jpg" width="750px" title="px(픽셀) 크기 설정" alt="RubberDuck"></img><br/>
first_fit과 매우 비슷하다. 하지만 경우에 따라( ex) 점점 큰 메모리가 할당되는 경우) 탐색 시간을 줄이는 방법이 될 수 있다.   

***

## Explicit (명시적 가용리스트)

### first_fit

### next_fit



## 실행방법
  
***Handout files for students   
Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.   
May not be used, modified, or copied without permission.***   


Main Files:   


mm.{c,h}   
	: Your solution malloc package. mm.c is the file that you   
	will be handing in, and is the only file you should modify.   

mdriver.c   
	: The malloc driver that tests your mm.c file   

short{1,2}-bal.rep   
	: Two tiny tracefiles to help you get started.    

Makefile   	
	: Builds the driver
	
 ***
***Other support files for the driver***   
  

config.h   
: Configures the malloc lab driver   
fsecs.{c,h}   
: Wrapper function for the different timer packages   
clock.{c,h}   
: Routines for accessing the Pentium and Alpha cycle counters   
fcyc.{c,h}   
: Timer functions based on cycle counters   
ftimer.{c,h}   
: Timer functions based on interval timers and gettimeofday()   
memlib.{c,h}   
: Models the heap and sbrk function   

 ***
***Building and running the driver***   
   
To build the driver, type "make" to the shell.   
   
To run the driver on a tiny test trace:   
   
	unix> mdriver -V -f short1-bal.rep   
   
The -V option prints out helpful tracing and summary information.   
   
To get a list of the driver flags:   
   
	unix> mdriver -h   


