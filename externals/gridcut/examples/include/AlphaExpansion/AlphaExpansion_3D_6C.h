// Alpha-Expansion solver for discrete multi-label optimization on grids
// Written by Lenka Saidlova at the Czech Technical University in Prague

// This software is in the public domain. Where that dedication is not
// recognized, you are granted a perpetual, irrevocable license to copy
// and modify this file however you want.

#ifndef AlphaExpansion_3D_6C_H_
#define AlphaExpansion_3D_6C_H_

#include <GridCut/GridGraph_3D_6C.h>

#include <algorithm>
#include <functional>
#include <random>

//0:  [ 1, 0, 0]
//1:  [ 0, 1, 0]
//2:  [ 0, 0, 1]
#define ALPHAEXPANSION_NEIGHBORS 3
#define ALPHAEXPANSION_INFINITY 1000000

template<
	typename type_label, // Type used to represent labels
	typename type_cost,  // Type used to represent data and smoothness costs
	typename type_energy // Type used to represent resulting energy
>
class AlphaExpansion_3D_6C
{
public:
	// Function for representing the smoothness term.
	// typedef type_energy (*SmoothCostFn)(int, int, int, int);
	using SmoothCostFn = std::function< type_energy (int, int, int, int) >;
	SmoothCostFn smooth_fn;

	// Constructors of the algorithm.
	// See "README.txt" how to fill data and smoothness costs
	AlphaExpansion_3D_6C(int width, int height, int depth, int nLabels, type_cost *data, type_cost **smooth);
	//AlphaExpansion_3D_6C(int width, int height, int depth, int nLabels, type_cost *data, SmoothCostFn smooth_fn);
	AlphaExpansion_3D_6C(int width, int height, int depth, int nLabels, type_cost *data, SmoothCostFn smooth_fn);
	~AlphaExpansion_3D_6C(void);
	
	// Sets the all pixels to the given label
	void set_labels(type_label label);

	// Sets the new labeling.
	// The array has to have width*height*depth elements, one value for each pixel.
	void set_labeling(type_label* labeling);

	// Runs the minimization. 
	// The algorithm iterate over the labels in a fixed order, from 0 to k - 1
	// and it stops when no further improvement is possible.
	void perform();

	// Runs the minimization. 
	// The algorithm iterate over the labels in a fixed order, from 0 to k - 1
	// and it stops after the number of max_cycles.
	void perform(int max_cycles);

	// Runs the minimization. 
	// The algorithm iterate over the labels in a random order
	// and it stops when no further improvement is possible.
	void perform_random();

	// Runs the minimization. 
	// The algorithm iterate over the labels in a random order
	// and it stops after the number of max_cycles.
	void perform_random(int max_cycles);

	// Returns the resulting energy.
	type_energy get_energy(void);

	// Returns the final labeling.
	// The array has width*height*depth elements, one value for each pixel.
	type_label* get_labeling(void);

	// Returns the final label for the given coordinates
	type_label get_label(int x, int y, int z);

	// Returns the final label for the given pixel index
	type_label get_label(int pix);

private:
	int width;
	int height;
	int depth;
	int depth_step;
	int nLabels; 
	long int nPixels; 
	type_cost **smooth_cost;
	type_cost *data_cost;
	type_label *labeling;
	bool smooth_array;
	typedef GridGraph_3D_6C<type_cost,type_cost,type_cost> Grid;
	Grid* grid;

	type_cost get_data_cost(int pix,int lab) const;
	type_cost get_smooth_cost(int pix,int lab1,int lab2) const;	
	void perform(int max_cycles, bool random);
	void init_common(int width, int height, int depth, int nLabels, type_cost *data);
	type_energy perform_cycle(bool random);
	void create_grid_array(type_label alpha_label);
	void create_grid_fn(type_label alpha_label);
	void perform_label(type_label alpha_label);
	type_energy get_data_energy();
	type_energy get_smooth_energy();
	type_energy get_smooth_energy_fn();	
	void add_tlink(int pix, type_cost to_source, type_cost to_sink, type_cost *source, type_cost *sink);
	void add_nlink(int x, int y, int z, int nx, int ny, int nz, type_cost A, type_cost B, type_cost C, type_cost D, type_cost *source, type_cost *sink);
};

template<typename type_label, typename type_cost, typename type_energy>
type_cost AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::get_data_cost(int pix,int lab) const
{
	return data_cost[(pix)*nLabels + lab];
}

template<typename type_label, typename type_cost, typename type_energy>
type_cost AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::get_smooth_cost(int pix,int lab1,int lab2) const
{
	return smooth_cost[pix][(lab2)+(lab1)*nLabels]; 
}

template<typename type_label, typename type_cost, typename type_energy>
void AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::init_common(int width, int height, int depth, int nLabels, type_cost *data)
{
	this->width = width;
	this->height = height;
	this->depth = depth;
	this->depth_step = width*height;
	this->nLabels = nLabels;
	this->nPixels = width*height*depth;
	this->data_cost = data;
	
	labeling = new type_label[nPixels];
	std::fill(labeling, labeling + nPixels, 0);

}

template<typename type_label, typename type_cost, typename type_energy>
AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::AlphaExpansion_3D_6C(int width, int height, int depth, int nLabels, type_cost *data, type_cost **smooth)
{

	init_common(width, height, depth, nLabels, data);
	
	smooth_array = true;
	smooth_cost = smooth;
	
}

template<typename type_label, typename type_cost, typename type_energy>
//AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::AlphaExpansion_3D_6C(int width, int height, int depth, int nLabels, type_cost *data, SmoothCostFn smoothFn)
AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::AlphaExpansion_3D_6C(int width, int height, int depth, int nLabels, type_cost *data, SmoothCostFn smoothFn )
{
	init_common(width, height, depth, nLabels, data);
		
	smooth_array = false;
	smooth_cost = NULL;
	this->smooth_fn = smoothFn;
	
}

template<typename type_label, typename type_cost, typename type_energy>
void AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::set_labeling(type_label* new_labeling){

	this->labeling = new_labeling;
}

template<typename type_label, typename type_cost, typename type_energy>
void AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::set_labels(type_label label){

	std::fill(labeling, labeling + nPixels, label);
}


template<typename type_label, typename type_cost, typename type_energy>
type_energy AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::get_data_energy(){

	type_energy energy = (type_energy) 0;

	for ( int i = 0; i < nPixels; i++ ){
		energy += get_data_cost(i, labeling[i]);
	}

	return energy;
}

template<typename type_label, typename type_cost, typename type_energy>
type_energy AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::get_smooth_energy(){

	type_energy energy = (type_energy) 0;
	int pix;
	
	for (int z = 0; z < depth; z++){
		for ( int y = 0; y < height; y++ ){
			for ( int x = 0; x < width - 1; x++ ){
				pix = x+y*width+z*depth_step;
				energy += get_smooth_cost(ALPHAEXPANSION_NEIGHBORS*pix, labeling[pix], labeling[pix+1]);
			}
		}
	}

	for (int z = 0; z < depth; z++){
		for ( int y = 0; y < height - 1; y++ ){
			for ( int x = 0; x < width; x++ ){
				pix = x+y*width+z*depth_step;
				energy += get_smooth_cost(ALPHAEXPANSION_NEIGHBORS*pix+1, labeling[pix], labeling[pix+width]);
			}
		}
	}

	for (int z = 0; z < depth - 1; z++){
		for ( int y = 0; y < height; y++ ){
			for ( int x = 0; x < width; x++ ){
				pix = x+y*width+z*depth_step;
				energy += get_smooth_cost(ALPHAEXPANSION_NEIGHBORS*pix+2, labeling[pix], labeling[pix+depth_step]);
			}
		}
	}
	
	return energy;
}

template<typename type_label, typename type_cost, typename type_energy>
type_energy AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::get_smooth_energy_fn(){

	type_energy energy = (type_energy) 0;
	int pix, nPix;
	
	for (int z = 0; z < depth; z++){
		for ( int y = 0; y < height; y++ ){
			for ( int x = 0; x < width - 1; x++ ){
				pix = x+y*width+z*depth_step;
				nPix = pix + 1;
				energy += smooth_fn(pix, nPix, labeling[pix], labeling[nPix]);
			}
		}
	}

	for (int z = 0; z < depth; z++){
		for ( int y = 0; y < height - 1; y++ ){
			for ( int x = 0; x < width; x++ ){
				pix = x+y*width+z*depth_step;
				nPix = pix+width;
				energy += smooth_fn(pix, nPix, labeling[pix], labeling[nPix]);
			}
		}
	}

	for (int z = 0; z < depth - 1; z++){
		for ( int y = 0; y < height; y++ ){
			for ( int x = 0; x < width; x++ ){
				pix = x+y*width+z*depth_step;
				nPix = pix+depth_step;
				energy += smooth_fn(pix, nPix, labeling[pix], labeling[nPix]);
			}
		}
	}
	
	return energy;
}

template<typename type_label, typename type_cost, typename type_energy>
type_energy AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::get_energy(){

	if(smooth_array){
		return get_data_energy() + get_smooth_energy();
	}
	return get_data_energy() + get_smooth_energy_fn();
}

template<typename type_label, typename type_cost, typename type_energy>
void AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::perform(){

	perform(ALPHAEXPANSION_INFINITY, false);
}

template<typename type_label, typename type_cost, typename type_energy>
void AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::perform(int max_cycles){

	perform(max_cycles, false);
}

template<typename type_label, typename type_cost, typename type_energy>
void AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::perform_random(int max_cycles){

	perform(max_cycles, true);
}

template<typename type_label, typename type_cost, typename type_energy>
void AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::perform_random(){

	perform(ALPHAEXPANSION_INFINITY, true);
}

template<typename type_label, typename type_cost, typename type_energy>
void AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::perform(int max_cycles, bool random){

	int cycle = 1;
    type_energy new_energy = get_energy();
	type_energy old_energy = -1;

	while ((old_energy < 0 || old_energy > new_energy)  && cycle <= max_cycles){
        old_energy = new_energy;
        new_energy = perform_cycle(random);
		cycle++;   
    }
	
}

template<typename type_label, typename type_cost, typename type_energy>
type_energy AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::perform_cycle(bool random){

    std::random_device rd;
    std::mt19937 gen {rd()};
	
	int* order = new int[nLabels];
	for (int i = 0; i < nLabels; i++){ 
		order[i] = i;
	}
	
	if(random){	
		// std::random_shuffle(order, order + nLabels);	
		std::ranges::shuffle(order, order + nLabels, gen);
	}

	for (int i = 0;  i < nLabels;  i++ ){
		perform_label(order[i]);
	}
       
    return get_energy();
}

template<typename type_label, typename type_cost, typename type_energy>
void AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::perform_label(type_label alpha_label){

	grid = new Grid(width,height,depth);
	if(smooth_array){
		create_grid_array(alpha_label);
	}else{
		create_grid_fn(alpha_label);
	}
	grid->compute_maxflow();
	
	int pix = 0;
	for (int z = 0; z < depth; z++){
		for (int y = 0; y < height; y++){
			for (int x = 0; x < width; x++){

				if ( labeling[pix] != alpha_label ){
					if (!grid->get_segment(grid->node_id(x,y,z))){
						labeling[pix] = alpha_label;
					}
				}
				pix++;
			}
		}	
	}
	
	delete grid;
	
}

template<typename type_label, typename type_cost, typename type_energy>
void AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::create_grid_array(type_label alpha_label){

	int pix, nPix;
	int* indices = new int[ALPHAEXPANSION_NEIGHBORS];
	std::fill(indices, indices + ALPHAEXPANSION_NEIGHBORS, 0);
	
	type_cost *source = new type_cost[nPixels];
	type_cost *sink = new type_cost[nPixels];
	std::fill(source, source + nPixels, 0);
	std::fill(sink, sink + nPixels, 0);
	
	for(int z = 0; z < depth; z++){
		for (int y = 0; y < height; y++){
			for (int x = 0; x < width; x++){

				pix = z*depth_step + y*width + x;
				for (int i = 0; i < ALPHAEXPANSION_NEIGHBORS; i++){
					indices[i] = ALPHAEXPANSION_NEIGHBORS*pix + i;
				}

				if(labeling[pix] != alpha_label){

					add_tlink(pix, get_data_cost(pix, labeling[pix]), get_data_cost(pix, alpha_label), source, sink);	

					if (x < width - 1){				
						nPix = pix + 1;
						if(labeling[nPix] != alpha_label){
							add_nlink(x, y, z, 1, 0, 0, get_smooth_cost(indices[0], alpha_label, alpha_label), get_smooth_cost(indices[0], alpha_label, labeling[nPix]), 
								get_smooth_cost(indices[0], labeling[pix], alpha_label), get_smooth_cost(indices[0], labeling[pix], labeling[nPix]), source, sink);
						}else{
							add_tlink(pix, get_smooth_cost(indices[0], labeling[pix], alpha_label), get_smooth_cost(indices[0], alpha_label, labeling[nPix]), source, sink);
						}
					}
					if(y < height - 1){
						nPix = pix + width;
						if(labeling[nPix] != alpha_label ){
							add_nlink(x, y, z, 0, 1, 0, get_smooth_cost(indices[1], alpha_label, alpha_label), get_smooth_cost(indices[1], alpha_label, labeling[nPix]), 
								get_smooth_cost(indices[1], labeling[pix], alpha_label), get_smooth_cost(indices[1], labeling[pix], labeling[nPix]), source, sink);
						}else{
							add_tlink(pix, get_smooth_cost(indices[1], labeling[pix], alpha_label), get_smooth_cost(indices[1], alpha_label, labeling[nPix]), source, sink);
						}
					}
					if(z < depth - 1){
						nPix = pix + depth_step;
						if(labeling[nPix] != alpha_label ){
							add_nlink(x, y, z, 0, 0, 1, get_smooth_cost(indices[2], alpha_label, alpha_label), get_smooth_cost(indices[2], alpha_label, labeling[nPix]), 
								get_smooth_cost(indices[2], labeling[pix], alpha_label), get_smooth_cost(indices[2], labeling[pix], labeling[nPix]), source, sink);
						}else{
							add_tlink(pix, get_smooth_cost(indices[2], labeling[pix], alpha_label), get_smooth_cost(indices[2], alpha_label, labeling[nPix]), source, sink);
						}
					}
				}else{
					if (x < width - 1){				
						nPix = pix + 1;
						if(labeling[nPix] != alpha_label){
							add_tlink(nPix, get_smooth_cost(indices[0], alpha_label, labeling[nPix]), get_smooth_cost(indices[0], alpha_label, alpha_label), source, sink);
						}
					}
					if(y < height - 1){
						nPix = pix + width;
						if(labeling[nPix] != alpha_label){
							add_tlink(nPix, get_smooth_cost(indices[1], alpha_label, labeling[nPix]), get_smooth_cost(indices[1], alpha_label, alpha_label), source, sink);
						}
					}
					if(z < depth - 1){
						nPix = pix + depth_step;
						if(labeling[nPix] != alpha_label){
							add_tlink(nPix, get_smooth_cost(indices[2], alpha_label, labeling[nPix]), get_smooth_cost(indices[2], alpha_label, alpha_label), source, sink);
						}
					}
				}			
			}
		}	
	}

	for(int z = 0; z < depth; z++){
		for (int y = 0; y < height; y++){
			for (int x = 0; x < width; x++){
				pix = z*depth_step + y*width + x;
				grid->set_terminal_cap(grid->node_id(x,y,z),source[pix], sink[pix]);
			}
		}
	}
	delete [] source;
	delete [] sink;
}

template<typename type_label, typename type_cost, typename type_energy>
void AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::create_grid_fn(type_label alpha_label){

	int pix, nPix;
	
	type_cost *source = new type_cost[nPixels];
	type_cost *sink = new type_cost[nPixels];
	std::fill(source, source + nPixels, 0);
	std::fill(sink, sink + nPixels, 0);
	
	for(int z = 0; z < depth; z++){
		for (int y = 0; y < height; y++){
			for (int x = 0; x < width; x++){

				pix = z*depth_step + y*width + x;

				if(labeling[pix] != alpha_label){

					add_tlink(pix, get_data_cost(pix, labeling[pix]), get_data_cost(pix, alpha_label), source, sink);	

					if (x < width - 1){				
						nPix = pix + 1;
						if(labeling[nPix] != alpha_label){
							add_nlink(x, y, z, 1, 0, 0, smooth_fn(pix, nPix, alpha_label, alpha_label), smooth_fn(pix, nPix, alpha_label, labeling[nPix]), 
								smooth_fn(pix, nPix, labeling[pix], alpha_label), smooth_fn(pix, nPix, labeling[pix], labeling[nPix]), source, sink);
						}else{
							add_tlink(pix, smooth_fn(pix, nPix, labeling[pix], alpha_label), smooth_fn(pix, nPix, alpha_label, labeling[nPix]), source, sink);
						}
					}

					if(y < height - 1){
						nPix = pix + width;
						if(labeling[nPix] != alpha_label ){
							add_nlink(x, y, z, 0, 1, 0, smooth_fn(pix, nPix, alpha_label, alpha_label), smooth_fn(pix, nPix, alpha_label, labeling[nPix]), 
								smooth_fn(pix, nPix, labeling[pix], alpha_label), smooth_fn(pix, nPix, labeling[pix], labeling[nPix]), source, sink);
						}else{
							add_tlink(pix, smooth_fn(pix, nPix, labeling[pix], alpha_label), smooth_fn(pix, nPix, alpha_label, labeling[nPix]), source, sink);
						}
					}
					if(z < depth - 1){
						nPix = pix + depth_step;
						if(labeling[nPix] != alpha_label ){
							add_nlink(x, y, z, 0, 0, 1, smooth_fn(pix, nPix, alpha_label, alpha_label), smooth_fn(pix, nPix, alpha_label, labeling[nPix]), 
								smooth_fn(pix, nPix, labeling[pix], alpha_label), smooth_fn(pix, nPix, labeling[pix], labeling[nPix]), source, sink);
						}else{
							add_tlink(pix, smooth_fn(pix, nPix, labeling[pix], alpha_label), smooth_fn(pix, nPix, alpha_label, labeling[nPix]), source, sink);
						}
					}
				}else{
					if (x < width - 1){				
						nPix = pix + 1;
						if(labeling[nPix] != alpha_label){
							add_tlink(nPix, smooth_fn(pix, nPix, alpha_label, labeling[nPix]), smooth_fn(pix, nPix, alpha_label, alpha_label), source, sink);
						}
					}
					if(y < height - 1){
						nPix = pix + width;
						if(labeling[nPix] != alpha_label){
							add_tlink(nPix, smooth_fn(pix, nPix, alpha_label, labeling[nPix]), smooth_fn(pix, nPix, alpha_label, alpha_label), source, sink);
						}
					}
					if(z < depth - 1){
						nPix = pix + depth_step;
						if(labeling[nPix] != alpha_label){
							add_tlink(nPix, smooth_fn(pix, nPix, alpha_label, labeling[nPix]), smooth_fn(pix, nPix, alpha_label, alpha_label), source, sink);
						}
					}
				}
			}			
		}
	}	

	for (int z = 0; z < depth; z++){
		for (int y = 0; y < height; y++){
			for (int x = 0; x < width; x++){
				pix = z*depth_step + y*width + x;
				grid->set_terminal_cap(grid->node_id(x,y,z),source[pix], sink[pix]);
			}
		}
	}
	delete [] source;
	delete [] sink;
}

template<typename type_label, typename type_cost, typename type_energy>
void AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::add_nlink(int x, int y, int z, int nx, int ny, int nz, type_cost A, type_cost B, type_cost C, type_cost D, type_cost *source, type_cost *sink){
	
	int pix = z*depth_step + y*width + x;
	int nPix = (z+nz)*depth_step + (y+ny)*width + (x+nx);
			
	if ( A+D > C+B) {
		type_cost delta = A+D-C-B;
        type_cost subtrA = delta/3;

        A = A-subtrA;
        C = C+subtrA;
        B = B+(delta-subtrA*2);
	}

	add_tlink(pix, D, A, source, sink);
    B -= A; 
	C -= D;
	
    if (B < 0){
		
		add_tlink(pix, -B, 0, source, sink);
		add_tlink(nPix, 0, -B, source, sink);

		grid->set_neighbor_cap(grid->node_id(x,y,z),	 nx, ny, nz, 0);
		grid->set_neighbor_cap(grid->node_id(x+nx,y+ny,z+nz), -nx, -ny, -nz, B+C); 

    }else if (C < 0){
        
		add_tlink(pix, 0, -C, source, sink);
		add_tlink(nPix, -C, 0, source, sink);

		grid->set_neighbor_cap(grid->node_id(x,y,z),	 nx, ny, nz, B+C);
		grid->set_neighbor_cap(grid->node_id(x+nx,y+ny,z+nz), -nx, -ny, -nz, 0); 

    }else{

		grid->set_neighbor_cap(grid->node_id(x,y,z),	 nx, ny, nz, B);
		grid->set_neighbor_cap(grid->node_id(x+nx,y+ny,z+nz), -nx, -ny, -nz, C); 
    }
}

template<typename type_label, typename type_cost, typename type_energy>
void AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::add_tlink(int pix, type_cost to_source, type_cost to_sink, type_cost *source, type_cost *sink){

	source[pix] += to_source;
	sink[pix] += to_sink;
}

template<typename type_label, typename type_cost, typename type_energy>
type_label * AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::get_labeling(void){

	return labeling;
}

template<typename type_label, typename type_cost, typename type_energy>
type_label AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::get_label(int x, int y, int z){

	return labeling[z*depth_step + y*width + x];
}

template<typename type_label, typename type_cost, typename type_energy>
type_label AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::get_label(int pix){

	return labeling[pix];
}

template<typename type_label, typename type_cost, typename type_energy>
AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::~AlphaExpansion_3D_6C(void){

	delete [] labeling;
	// delete [] data_cost;
	// if(smooth_array)
	// 	delete [] smooth_cost;
}

#undef ALPHAEXPANSION_NEIGHBORS
#undef ALPHAEXPANSION_INFINITY

#endif
