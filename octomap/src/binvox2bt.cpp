/*
 * OctoMap - An Efficient Probabilistic 3D Mapping Framework Based on Octrees
 * http://octomap.github.com/
 *
 * Copyright (c) 2009-2013, K.M. Wurm and A. Hornung, University of Freiburg
 * All rights reserved.
 * License: New BSD
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University of Freiburg nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*!
* @file binvox2bt.cpp
* Read files generated by Patrick Min's 3D mesh voxelizer
* ("binvox", available at: http://www.cs.princeton.edu/~min/binvox/)
* and convert the voxel meshes to a single bonsai tree file.
* This file is based on code from http://www.cs.princeton.edu/~min/binvox/read_binvox.cc
*
* @author S. Osswald, University of Freiburg, Copyright (C) 2010.
* License: New BSD License
*/

#ifndef M_PI_2
  #define M_PI_2 1.5707963267948966192E0
#endif

#include <string>
#include <fstream>
#include <iostream>
#include <octomap/octomap.h>
#include <cstdlib>
#include <cstring>

using namespace std;
using namespace octomap;
typedef unsigned char my_byte;

int main(int argc, char **argv)
{
    int version;               // binvox file format version (should be 1)
    int depth, height, width;  // dimensions of the voxel grid
    int size;                  // number of grid cells (height * width * depth)
    float tx, ty, tz;          // Translation
    float scale;               // Scaling factor
    bool mark_free = false;    // Mark free cells (false = cells remain "unknown")    
    bool rotate = false;       // Fix orientation of webots-exported files
    bool show_help = false;
    string output_filename;
    double minX = 0.0;
    double minY = 0.0;
    double minZ = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
    double maxZ = 0.0;
    bool applyBBX = false;
    bool applyOffset = false;
    octomap::point3d offset(0.0, 0.0, 0.0);
    OcTree *tree = 0;

    if(argc == 1) show_help = true;
    for(int i = 1; i < argc && !show_help; i++) {
        if(strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-help") == 0 ||
           strcmp(argv[i], "--usage") == 0 || strcmp(argv[i], "-usage") == 0 ||
           strcmp(argv[i], "-h") == 0
          )
               show_help = true;
    }
    
    if(show_help) {
        cout << "Usage: "<<argv[0]<<" [OPTIONS] <binvox filenames>" << endl;
        cout << "\tOPTIONS:" << endl;
        cout << "\t -o <file>        Output filename (default: first input filename + .bt)\n";
        cout << "\t --mark-free      Mark not occupied cells as 'free' (default: unknown)\n";
        cout << "\t --rotate         Rotate left by 90 deg. to fix the coordinate system when exported from Webots\n";
        cout << "\t --bb <minx> <miny> <minz> <maxx> <maxy> <maxz>: force bounding box for OcTree\n";
        cout << "\t --offset <x> <y> <z>: add an offset to the final coordinates\n";
        cout << "If more than one binvox file is given, the models are composed to a single bonsai tree->\n";
        cout << "All options apply to the subsequent input files.\n\n";
        exit(0);
    }
    
    for(int i = 1; i < argc; i++) {
        // Parse command line arguments    
        if(strcmp(argv[i], "--mark-free") == 0) {
            mark_free = true;
            continue;
        } else if(strcmp(argv[i], "--no-mark-free") == 0) {
            mark_free = false;
            continue;
        }else if(strcmp(argv[i], "--rotate") == 0) {
          rotate = true;
          continue;
        } else if(strcmp(argv[i], "-o") == 0 && i < argc - 1) {            
            i++;
            output_filename = argv[i];

            continue;
        } else if (strcmp(argv[i], "--bb") == 0 && i < argc - 7) {
          i++;
          minX = atof(argv[i]);
          i++;
          minY = atof(argv[i]);
          i++;
          minZ = atof(argv[i]);
          i++;
          maxX = atof(argv[i]);
          i++;
          maxY = atof(argv[i]);
          i++;
          maxZ = atof(argv[i]);

          applyBBX = true;

          continue;
        } else if (strcmp(argv[i], "--offset") == 0 && i < argc - 4) {
        	i++;
        	offset(0) = (float) atof(argv[i]);
        	i++;
        	offset(1) = (float) atof(argv[i]);
        	i++;
        	offset(2) = (float) atof(argv[i]);

        	applyOffset = true;

        	continue;
        }


        // Open input file
        ifstream *input = new ifstream(argv[i], ios::in | ios::binary);    
        if(!input->good()) {
            cerr << "Error: Could not open input file " << argv[i] << "!" << endl;
            exit(1);
        } else {
            cout << "Reading binvox file " << argv[i] << "." << endl;
            if(output_filename.empty()) { 
                output_filename = string(argv[i]).append(".bt");
            }
        }

        // read header
        string line;
        *input >> line;    // #binvox
        if (line.compare("#binvox") != 0) {
            cout << "Error: first line reads [" << line << "] instead of [#binvox]" << endl;
            delete input;
            return 0;
        }
        *input >> version;
        cout << "reading binvox version " << version << endl;

        depth = -1;
        int done = 0;
        while(input->good() && !done) {
            *input >> line;
            if (line.compare("data") == 0) done = 1;
            else if (line.compare("dim") == 0) {
                *input >> depth >> height >> width;
            }
            else if (line.compare("translate") == 0) {
                *input >> tx >> ty >> tz;
            }
            else if (line.compare("scale") == 0) {
                *input >> scale;
            }
            else {
                cout << "    unrecognized keyword [" << line << "], skipping" << endl;
                char c;
                do {    // skip until end of line
                    c = input->get();
                } while(input->good() && (c != '\n'));

            }
        }
        if (!done) {
            cout << "    error reading header" << endl;
            return 0;
        }
        if (depth == -1) {
            cout << "    missing dimensions in header" << endl;
            return 0;
        }

        size = width * height * depth;
        int maxSide = std::max(std::max(width, height), depth);
        double res = double(scale)/double(maxSide);

        if(!tree) {
            cout << "Generating octree with leaf size " << res << endl << endl;
            tree = new OcTree(res);
        }

        if (applyBBX){
          cout << "Bounding box for Octree: [" << minX << ","<< minY << "," << minZ << " - "
              << maxX << ","<< maxY << "," << maxZ << "]\n";

        }
        if (applyOffset){
        	std::cout << "Offset on final map: "<< offset << std::endl;

        }
                
        cout << "Read data: ";
        cout.flush();
            
        // read voxel data
        my_byte value;
        my_byte count;
        int index = 0;
        int end_index = 0;
        unsigned nr_voxels = 0;
        unsigned nr_voxels_out = 0;
        
        input->unsetf(ios::skipws);    // need to read every byte now (!)
        *input >> value;    // read the linefeed char

        while((end_index < size) && input->good()) {
            *input >> value >> count;

            if (input->good()) {
                end_index = index + count;
                if (end_index > size) return 0;
                for(int i=index; i < end_index; i++) {
                    // Output progress dots
                    if(i % (size / 20) == 0) {
                        cout << ".";            
                        cout.flush();
                    }
                    // voxel index --> voxel coordinates 
                    int y = i % width;
                    int z = (i / width) % height;
                    int x = i / (width * height);
                    
                    // voxel coordinates --> world coordinates
                    point3d endpoint((float) ((double) x*res + tx + 0.000001), 
                                     (float) ((double) y*res + ty + 0.000001),
                                     (float) ((double) z*res + tz + 0.000001));

                    if(rotate) {
                      endpoint.rotate_IP(M_PI_2, 0.0, 0.0);
                    }
                    if (applyOffset)
                    	endpoint += offset;
                    
                    if (!applyBBX  || (endpoint(0) <= maxX && endpoint(0) >= minX
                                   && endpoint(1) <= maxY && endpoint(1) >= minY
                                   && endpoint(2) <= maxZ && endpoint(2) >= minZ)){

                      // mark cell in octree as free or occupied
                      if(mark_free || value == 1) {
                          tree->updateNode(endpoint, value == 1, true);
                      }
                    } else{
                      nr_voxels_out ++;
                    }
                }
                
                if (value) nr_voxels += count;
                index = end_index;
            }    // if file still ok
            
        }    // while
        
        cout << endl << endl;

        input->close();
        cout << "    read " << nr_voxels << " voxels, skipped "<<nr_voxels_out << " (out of bounding box)\n\n";
    }
    
    // prune octree
    cout << "Pruning octree" << endl << endl;
    tree->updateInnerOccupancy();
    tree->prune();
 
    // write octree to file  
    if(output_filename.empty()) {
        cerr << "Error: No input files found." << endl << endl;
        exit(1);
    }

    cout << "Writing octree to " << output_filename << endl << endl;
   
    tree->writeBinary(output_filename.c_str());

    cout << "done" << endl << endl;
    return 0;
}
