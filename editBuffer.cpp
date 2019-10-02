
/*
This should give an idea for how to do fast, cross-chunk manipulations for large world-sizes.

World Class: Contains a bunch of functions
bufferObject Struct: Helps handle the changes you want to make.

The world contains a sortable container of bufferObjects. We generate a blank world and save it to file. Then we do as many edits as we want to the world, saving all these edits to file.
Finally, we evaluate all edits at a single time, in a way that we only need to do a single file pass over the world file.

This is also nice because you can just keep track of the editbuffer, and when new chunks are loaded or chunks are unloaded, write the editbuffer to memory, which should contain small amounts of changes and thus be fast.
Live viewing of block changes then happens by also changing the chunk information in the virtual memory (i.e. container of loaded chunks)

Many benefits from using this exact structure
*/


//EditBuffer Struct
struct bufferObject {
  int chunkSize;
  glm::vec3 pos;  //Position in WorldSpace
  glm::vec3 cpos; //Position in Chunkspace
  BlockType type; //Some type information (this could actually be an object of information that a block contains)
};

class World{
  std::vector<Chunk> chunks; //Loaded Chunks (will not be used here at all)
  int chunkSize = 16; //Size of one chunk (does not need to be same size on all sides in theory)
  glm::vec3 dim = glm::vec3(20, 5, 20); //World Dimensions in Chunks
  
  void generate();        //Generate the World (call this)
  void generateBlank();   //Generate blank chunks and save to file
  void addCrap1();        //A bunch of functions for adding what you like to your arbitrarily large world, already saved on disk
  void addCrap2();        //e.g. add a floor, then trees, then this, then that, etc.
  void addCrap3();        //
  
  std::vector<bufferObject> editBuffer; //Sortable container of bufferobjects
  bool addEditBuffer(glm::vec3 _pos, BlockType _type); //Function for adding something to the editBuffer
  bool evaluateEditBuffer();   //Function for writing all requested changes to file.
}

void World::generate(){
  generateBlank();
  addCrap1();
  addCrap2();
  addCrap3();
  //...
}

void World::generateBlank(){
  //Add Blank Chunks to Region Files
  //Loop over all Chunks in the World
  for(int i = 0; i < dim.x; i++){
    for(int j = 0; j < dim.y; j++){
      for(int k = 0; k < dim.z; k++){

        //Generate a new chunk at a specific location
        Chunk chunk;
        
        //Set chunk information
        chunk.biome = BIOME_VOID;
        chunk.size = chunkSize;
        
        //Set the chunks position in chunkspace
        chunk.pos = glm::vec3(i, j, k);

        //Write the Chunk to File
        writeChunk(chunk); //This just appends the chunk to a "world.region" file, as a serialized object in a new line.
        
        //In file, chunks are thereby sorted! there is an explicit order in which they are written in.
      }
    }
  }
}

void World::addCrap1(){
  /*
  Important is that all requested edits are done in worldspace!
  Who cares about chunks?
  */
  
  //You decide what happens here!!! Do anything you want. Example: Add a rock
  std::cout<<"Adding Rocks"<<std::endl;
  
  for(int i = 0; i < 1000; i++){  //Add 1000 rocks
    int rock[2] = {rand()%(chunkSize*(int)dim.x), rand()%(chunkSize*(int)dim.z)}; //Choose a random position in worldspace
    int height = getHeight(rock[0], rock[1]); //Somehow you need to define this if you want to place stuff on surface

    //Example of adding to the edit buffer. Again, you can pass anything else to this function if its information about a block
    addEditBuffer(glm::vec3(rock[0], (int)height, rock[1]), BLOCK_STONE);
  }

  //Evaluate the Guyd
  evaluateEditBuffer();
}

bool World::addEditBuffer(glm::vec3 _pos, BlockType _type){
  //Check validity
  if(glm::any(glm::lessThan(_pos, glm::vec3(0))) || glm::any(glm::greaterThanEqual(_pos, glm::vec3(chunkSize)*dim))){
    //Invalid Position
    return false;
  }

  //Add a new bufferObject
  editBuffer.push_back(bufferObject());
  editBuffer.back().pos = _pos;
  editBuffer.back().cpos = glm::floor(_pos/glm::vec3(chunkSize));
  editBuffer.back().type = _type;

  //Push it on
  return true;
}

/*
Our goal now is to open the world.region file, and do a single pass through it. 
Because we know the chunks are sorted in the file, we should sort our editBuffer so we can do all changes in the line of the file as we visit it. 
This is done as follows:
*/


/*
These operators sort a bufferObject according to the order of the chunk they belong to in file!
*/

//Sorting Operator for bufferObjects
bool operator<(const bufferObject& a, const bufferObject& b) {
  if(a.cpos.x < b.cpos.x) return true;
  if(a.cpos.x > b.cpos.x) return false;

  if(a.cpos.y < b.cpos.y) return true;
  if(a.cpos.y > b.cpos.y) return false;

  if(a.cpos.z < b.cpos.z) return true;
  if(a.cpos.z > b.cpos.z) return false;

  return false;
}

//Sorting Operator for bufferObjects
bool operator>(const bufferObject& a, const bufferObject& b) {
  if(a.cpos.x > b.cpos.x) return true;
  if(a.cpos.x < b.cpos.x) return false;

  if(a.cpos.y > b.cpos.y) return true;
  if(a.cpos.y < b.cpos.y) return false;

  if(a.cpos.z > b.cpos.z) return true;
  if(a.cpos.z < b.cpos.z) return false;

  return false;
}


/*
Sort the bufferobject, open the world file and a new file, go through every line, loading chunks, writing edits, and writing the chunks back to the new file.
*/

bool World::evaluateEditBuffer(){
  //Check if the editBuffer isn't empty!
  if(editBuffer.empty()){
    std::cout<<"editBuffer is empty."<<std::endl;
    return false;
  }

  //Sort the editBuffer
  std::sort(editBuffer.begin(), editBuffer.end(), std::greater<bufferObject>());

  //Open the File
  boost::filesystem::path data_dir(boost::filesystem::current_path());
  data_dir /= "save";
  data_dir /= saveFile;

  //Load File and Write File
  std::ifstream in((data_dir/"world.region").string());
  std::ofstream out((data_dir/"world.region.temp").string());

  //Chunk for Saving Data
  Chunk _chunk;

  int n_chunks = 0;

  //While there is still stuff inside the editBuffer...
  while(!editBuffer.empty()){
    //Read the File into the Chunk

    {
      boost::archive::text_iarchive ia(in);
      ia >> _chunk;
    }

    //If the chunk is not equal to the editbuffer's element (i.e. this chunk has no changes)
    while(_chunk.pos != editBuffer.back().cpos){
      //Make sure there is an endoffile criterion.
      if(in.eof()){
        std::cout<<"Error: Reached end of file."<<std::endl;
        return false;
      }

      //Write the chunk to the new file, and load a new chunk.
      {
        boost::archive::text_oarchive oa(out);
        oa << _chunk;
        n_chunks++;
        boost::archive::text_iarchive ia(in);
        ia >> _chunk;
      }
    }

    //Now we have a chunk that corresponds the the editBuffer element (i.e. this chunk has changes)
    while(!editBuffer.empty() && _chunk.pos == editBuffer.back().cpos){
      //Set the block in the chunk.
      _chunk.setPosition(glm::mod(editBuffer.back().pos, glm::vec3(chunkSize)), editBuffer.back().type);

      //Remove this edit from the list.
      editBuffer.pop_back();
    }

    //Write the edited chunk to file
    {
      boost::archive::text_oarchive oa(out);
      oa << _chunk;
      n_chunks++;
    }
    
    //Repeat until the editBuffer is empty!
  }

  //Fill up the rest, if there are any chunks at the end with no edits
  while(n_chunks < dim.x*dim.y*dim.z){
    boost::archive::text_iarchive ia(in);
    boost::archive::text_oarchive oa(out);
    oa << _chunk;
    ia >> _chunk;
    n_chunks++;
  }

  //Close the fstream and ifstream
  in.close();
  out.close();

  //Delete the first file, rename the temp file
  boost::filesystem::remove_all((data_dir/"world.region").string());
  boost::filesystem::rename((data_dir/"world.region.temp").string(),(data_dir/"world.region").string());

  //Success!
  return true;
}
