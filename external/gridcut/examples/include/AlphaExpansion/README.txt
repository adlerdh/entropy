###########################################################################
#                                                                         #
#    AlphaExpansion - software for energy minimization with graph cuts    #
#                                                                         #
###########################################################################

1. Introduction

This software library implements the Graph Cuts Energy Minimization methods described in

  [1] Efficient Approximate Energy Minimization via Graph Cuts 
      Yuri Boykov, Olga Veksler, Ramin Zabih, 
      IEEE Transactions on Pattern Analysis and Machine Intelligence, 23(11):1222-1239, 2001. 

  [2] What Energy Functions can be Minimized via Graph Cuts?
      Vladimir Kolmogorov and Ramin Zabih,
      IEEE Transactions on Pattern Analysis and Machine Intelligence, 26(2):147-159, 2004.        


#########################################################################################################################

2. Definitions

    Variable      Meaning                             Calculated 
                                                                                            
    w             Width of a grid       
    h             Height of a grid 
    d             Depth of a grid 
    n             Number of pixels (voxels)           wh in 2D, whd in 3D
    NEIGHBORS     Number of the pixel's neighbors
    x             Coordinate in X axis
    y             Coordinate in Y axis
    z             Coordinate in Z axis
    d_step        Depth step                          wh
    p_i           Pixel's index in a grid             y*w+x in 2D, z*d_step+y*w+x in 3D
    k             Number of labels

        
#########################################################################################################################

3. Initialization

  The AlphaExpansion is header-only library and doesn't need to be compiled separately. There is a single header file 
  for each grid topology. In order to start using AlphaExpansion, we have to add the "./include" directory to compiler's 
  include path and \#include the required header file in the code. The multi-threaded "MT" versions are AlphaExpansion 
  implementations calling the parallel version of GridCut.

    #include <AlphaExpansion_2D_4C.h>
    #include <AlphaExpansion_2D_4C_MT.h>

    #include <AlphaExpansion_2D_8C.h>

    #include <AlphaExpansion_3D_6C.h>
    #include <AlphaExpansion_3D_6C_MT.h>

    #include <AlphaExpansion_3D_26C.h>

  We allow to choose different data types of labels, smoothness, data and energy costs by template mechanism. 
  The first parameter of the template is data type for labels, the second is for data and smoothness costs 
  and the third parameter is for the resulting energy:

    template<typename type_label, typename type_cost, typename type_energy>


#########################################################################################################################

4. Setting the data and smoothness terms.
  
  4.1 Data costs
                  
    We expect that data costs are provided by the array of n*k elements, i.e. the array contains k values for each pixel. 
    The values are ordered in the way, that for the pixel index p_i and the label l, the data cost value is 
    on the array index p_i*k+l. 
  
      l_0(p_0) | l_1(p_0) | ... | l_{k-1}(p_0) | l_0(p_1) | l_1(p_1) | ... | l_{k-1}(p_1) | ... | ... | l_{k-1}(p_{n-1})
   

  4.2 Smoothness costs

  (a) Function:

      typedef type_energy (*SmoothCostFn)(int pix1, int pix2, int lab1, int lab2);
      SmoothCostFn smooth_fn;
  
    The parameters of the function are two neighboring pixels pix1, pix2 and their labels lab1, lab2. The return value 
    has to be of a data type for the resulting energy. 

  (b) One smoothness table:
    
    The smoothness table is a 1D array of k^2 elements defining smoothness costs for all the pairs of neighboring pixels 
    depending on their labels. For two neighboring pixels with indexes p, q and label l, the corresponding smoothness cost 
    lies on index p*k+l in the table. 
  
                  l_0(q)    l_1(q)    l_2(q)    ...   l_{k-1}(q) 
                
      l_0(p)      0         1         2         ...   k-1
      l_1(p)      k         k+1       k+2       ...   2k-1
      l_2(p)      2k        2k+1      2k+2      ...   3k-1
      .
      .
      .
      l_{k-1}(p)  k(k-1)   k(k-1)+1  k(k-1)+2  ...   k^2-1
 
  (c) Multiple smoothness tables
  
    When we want to provide different smoothness tables for each pair of neighbors, we need to provide smoothness costs 
    as an array of n*NEIGHBORS/2 smoothness tables. All these tables are in such order that there are NEIGHBORS/2 tables 
    for the first pixel, then NEIGHBORS/2 tables for the second pixel etc. Thus, for the pixel p with index p_i, 
    its first smoothness table lies on index p_i*NEIGHBORS/2. 
    
    S stands for one smoothness table and t is number of smoothness tables for each pixel.
  
      S_0(p_0) | S_1(p_0) | ... | S_{t-1}(p_0) | S_0(p_1) | S_1(p_1) | ... | S_{t-1}(p_1) | ... | ... | S_{t-1}(p_{n-1})

    For 2D grids with 4 connected neighboring system, we expect two tables for each pixel. The first table corresponds 
    to the pair of the pixel p and its right neighbor and the second table corresponds to the pair of its bottom neighbor. 
    
      Table    Offset X    Offset Y 
      1        +1           0 
      2         0          +1 

    For 2D grids with 8 connected neighboring system, we expect four tables for each pixel. The first two tables 
    are the same as before and there are two other tables for diagonal neighbors. 

      Table    Offset X    Offset Y  
      1        +1           0 
      2         0          +1
      3        +1          -1
      4        +1          +1

    For 3D grids with 6 connected neighboring system, we expect three smoothness tables for each pixel.

      Table    Offset X    Offset Y    Offset Z 
      1        +1           0           0
      2         0          +1           0 
      3         0           0          +1

    For 3D grids with 26 connected neighboring system, we expect thirteen smoothness tables for each pixel.
        
      Table    Offset X    Offset Y    Offset Z 
      1        +1           0           0 
      2         0          +1           0 
      3        +1          -1           0 
      4        +1          +1           0 
      5         0           0          +1
      6         0          -1          +1 
      7         0          +1          +1 
      8        -1           0          +1 
      9        -1          -1          +1 
      10       -1          +1          +1
      11       +1           0          +1 
      12       +1          -1          +1
      13       +1          +1          +1 


#########################################################################################################################

5. Constructors

  5.1 2D with 4 connected neighboring system

    AlphaExpansion_2D_4C(int width, int height, int n_labels, type_cost *data, SmoothCostFn smooth_fn);
    AlphaExpansion_2D_4C(int width, int height, int n_labels, type_cost *data, type_cost *smooth);
    AlphaExpansion_2D_4C(int width, int height, int n_labels, type_cost *data, type_cost **smooth);

  For all cases, the constructor parameters are width and height of the grid (image), number of labels 
  and a 1D array with data costs. There are three ways setting the smoothness costs. 
  In the first case, smoothness costs are given by the function with properties we defined earlier. 
  In the second case, smoothness costs are given by a 1D array, which represents one smoothness table for all pairs of neighbors.
  In the third case, smoothness costs are given by 2D array, which represents 2*n smoothness tables, i.e. two smoothness tables 
  for each pixel. 

  For "MT" version:

    AlphaExpansion_2D_4C_MT(int width, int height, int n_labels, type_cost *data, SmoothCostFn smooth_fn, int num_threads, int block_size);
    AlphaExpansion_2D_4C_MT(int width, int height, int n_labels, type_cost *data, type_cost *smooth, int num_threads, int block_size);
    AlphaExpansion_2D_4C_MT(int width, int height, int n_labels, type_cost *data, type_cost **smooth, int num_threads, int block_size);
  
  The parameter num_threads represents how many threads GridCut will use and parameter block_size representing the size of blocks
  in memory, which GridCut will allocate. See [3] for details
   
    [3] Cache-efficient Graph Cuts on Structured Grids
        Ondrej Jamriska, Daniel Sykora, Alexander Hornung,
        IEEE Conference on Computer Vision and Pattern Recognition 2012.


  5.2 2D with 8 connected neighboring system  
    
    AlphaExpansion_2D_8C(int width, int height, int n_labels, type_cost *data, SmoothCostFn smooth_fn);
    AlphaExpansion_2D_8C(int width, int height, int n_labels, type_cost *data, type_cost **smooth);
  
  The first constructor is for the case of smoothness function. 
  The second constructor is for the case of smoothness tables. Smoothness costs are given by 4*n smoothness tables, 
  which means four tables for each pixel. 
  
  
  5.3 3D with 6 connected neighboring system  
  
    AlphaExpansion_3D_6C(int width, int height, int depth, int n_labels, type_cost *data, SmoothCostFn smooth_fn);
    AlphaExpansion_3D_6C(int width, int height, int depth, int n_labels, type_cost *data, type_cost **smooth);

    AlphaExpansion_3D_6C_MT(int width, int height, int depth, int n_labels, type_cost *data, SmoothCostFn smooth_fn, int num_threads, int block_size);
    AlphaExpansion_3D_6C_MT(int width, int height, int depth, int n_labels, type_cost *data, type_cost **smooth, int num_threads, int block_size);

  The constructors are similar to the 2D case. We only added the parameter of a grid's depth and we expect 
  different number of tables, which is 3*n. For each pixel, there has to be three tables for each pixel. 


  5.4 3D with 26 connected neighboring system  

    AlphaExpansion_3D_26C(int width, int height, int depth, int n_labels, type_cost *data, SmoothCostFn smooth_fn);
    AlphaExpansion_3D_26C(int width, int height, int depth, int n_labels, type_cost *data, type_cost **smooth);
  
  The only difference is in the number of expected tables, which is 13*n smoothness tables. 
  

#########################################################################################################################

6. Setting initial labeling 
  
  Since the algorithm is sensitive to initial labeling, we allow to set it by these functions:
  
    void set_labeling(type_label* labeling);
    void set_labels(type_label label);
  
  The first function expects an array of n elements, where a label for each pixel is set. The second function
  allows to set the given label to all pixels. 
  If neither of these functions is called, the initial labeling is set to (type_label)0 for all the  pixels.


#########################################################################################################################

7. Running the algorithm

    void perform();
    void perform(int max_cycles);
    void perform_random();
    void perform_random(int max_cycles);

  If the algorithm is supposed to iterate over the labels in a fixed order, from 0 to k-1, we call one of the first two functions.
  If we set the parameter max_cycles, the algorithm stops after this number of cycles. Otherwise, it stops 
  when no further improvement is possible. When we want to iterate over the labels in a random order, 
  we call one of the remaining functions. The parameter meaning remains the same.


#########################################################################################################################


8. Getting the result

    type_label* get_labeling(void);
    type_label get_label(int pix);
    type_label get_label(int x, int y);
    type_label get_label(int x, int y, int z);

  The first function returns the final labeling, i.e. a 1D array of n elements with one value for each pixel.
  The second function returns a single label for the given index of pixel and the third and the fourth function
  return a single label too, but the parameters are the coordinates of the pixel in a 2D or in a 3D grid.

  If we need to get the final energy, we can call the function

    type_energy get_energy(void);


#########################################################################################################################


9. EXAMPLES

  Example 1 - Smoothness Term as Multiple Tables

  The first example focuses on how to work with a 2D grid of 4 connected neighboring system and how to initialize all the costs.
  First we present a function which fills the array of data costs:
 
    void get_data_costs(unsigned char *image, unsigned char *labels, int n_labels, int width, int height, int *data){
      int K = 1000;
      for (int pix = 0; pix < width*height; pix++)
      for (int j = 0; j < n_labels; j++){
        if(image[pix] == labels[j]) data[pix*n_labels + j] = 0;
        else data[pix*n_labels + j] = K;
      }
    }

  The parameter image represents the array of image pixels, the parameter labels represents the array of label values
  and the data array represents the data costs.

  Next, we present a function which fills the array of smoothness costs:

    void get_smooth_costs(unsigned char *image, unsigned char *labels, int n_labels, int width, int height, int **smooth){
      int pix;
      for (int y = 0; y < height; y++)
      for (int x = 0; x < width; x++){

        pix = y*width + x;
        //smoothness table for the right neighbor
        smooth[2*pix] = new int[n_labels*n_labels];
        if(x < width - 1){
          for (int j = 0; j < n_labels; j++)
            for (int k = 0; k < n_labels; k++){
              if(j == k) smooth[2*pix][n_labels*j+k] = 0;
              else smooth[2*pix][n_labels*j+k] = WEIGHT(img[pix]-img[pix+1]);
            }
        //just for sure, the pixel on the right border does not have a right neighbor
        } else {
          for (int j = 0; j < nLabels*nLabels; j++){
            smooth[2*pix][j] = 0;
          }
        }
        //smoothness table for the bottom neighbor
        smooth[2*pix+1] = new int[n_labels*n_labels];
        if(y < height - 1){
          for (int j = 0; j < n_labels; j++)
            for (int k = 0; k < n_labels; k++){
              if(j == k) smooth[2*pix+1][n_labels*j+k] = 0;
              else smooth[2*pix+1][n_labels*j+k] = WEIGHT(img[pix]-img[pix+w]);
            }
        //just for sure, the pixel on the bottom border does not have a bottom neighbor
        } else {
          for (int j = 0; j < n_labels*n_labels; j++){
            smooth[2*pix+1][j] = 0;
          }
        }
      }
    }

  The parameters of the function are the same except for the last one. That one represents the array of smoothness costs.
  The WEIGHT can represent any smoothness function we showed earlier. After we defined the functions, we can initialize
  the data cost array and the smoothness cost array:

    int *data = new int[width*height*n_labels];
    get_data_costs(image, labels, n_labels, width, height, data);
    
    int **smooth = new int*[2*width*height];
    get_smooth_costs(image, labels, n_labels, width, height, smooth);

  To run the algorithm, we simply call the corresponding constructor, call the perform function and get the final labeling:

    typedef AlphaExpansion_2D_4C<unsigned char, int, int> Expansion;
    Expansion *exp = new Expansion(w, h, length, data, smooth);
    exp->perform();
    unsigned char* final_labeling = exp->get_labeling();


  --------------------------------------------------------------------------------------------------------------------------

  Example 2 - Smoothness Term as a Function

  The second example shows how to use a smoothness function in practice. For this case, we show the example on a 3D grid 
  with 6 connected neighboring system for the multi-threaded version. First we define the smoothness function in the following way:

    int smoothFn(int pix1, int pix2, int label1, int label2) {
      if(label1 == label2) return 0;

      int weight = WEIGHT(img[pix1]-img[pix2]);
      int p_z = pix1%d_step;
      int q_z = pix2%d_step;

      //if q is the back neighbor of p, modify the weight
      if(p_z != q_z){
        weight = weight / 2;
      }
      return weight;
    }

  The variable d_step has to be a global variable. For the data costs, we can use the same function as before and we initialize
  the data cost in the same way.

  We already know the rest of use. We call the constructor, where we set 2 threads and set the size of blocks in memory to 160x160, 
  then we run the algorithm (in this case for random order of labels) and get the result:

    typedef AlphaExpansion_3D_6C_MT<unsigned char, int, int> Expansion;
    Expansion *exp = new Expansion(w, h, length, data, &smoothFn, 2, 160);
    exp->perform_random();
    unsigned char* final_labeling = exp->get_labeling();

