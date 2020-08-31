#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


// Constants about reading file 
const int MAX_LINE_LEN = 6000;
const int MAX_WORD_LEN = 20;
const int NUM_WORDS = 1000;
const int EMBEDDING_DIMENSION= 300;
const char DELIMITER[2] = "\t";

// Constants about commands
const int COMMAND_EXIT = 0;
const int COMMAND_QUERY = 1;
const int COMMAND_CALCULATE_SIMILARITY = 2;


// Finds the index of query_word in words
int findWordIndex(char *words, char *query_word){
   for(int wordIndex = 0; wordIndex<NUM_WORDS; wordIndex++){
    if(strcmp((words+wordIndex*MAX_WORD_LEN), query_word)==0){
     return wordIndex;
   }
 }
 return -1;
}


void runMasterNode(int rank, int wordCountPerProcess, int numOfProcessor){
  
  char line[MAX_LINE_LEN];
 
  FILE *file = fopen ("word_embeddings_1000.txt", "r");
  int wordIndex = 0;
 
  // embeddings_matrix = keeps the embedding matrix of words
  float* embeddings_matrix = (float*)malloc(sizeof(float) * (wordCountPerProcess)*EMBEDDING_DIMENSION); //embedding matrix için slave başına bu kadar yer ayırdı
  // words = keeps the words
  char* words = (char*)malloc(sizeof(char) * (wordCountPerProcess)*MAX_WORD_LEN); // words için slave başına bu kadar yer ayırdı


  // First *for* traverse the slave nodes, Second *fors*  reads each line of the file and puts words and their corresponding embedding matrices into appropriate array defined above
  // Then send the words and their embedding matrices to slaves 
  // File is read partially.
  for(int slaveRank = 1; slaveRank <= numOfProcessor-1; slaveRank++){
    for(int i = 0; i< wordCountPerProcess; i++){
      fgets(line, MAX_LINE_LEN, file);

      char *word;
      word = strtok(line, DELIMITER);
      strcpy(words+i*MAX_WORD_LEN, word); 

      for(int embIndex = 0; embIndex<EMBEDDING_DIMENSION; embIndex++){
         char *field = strtok(NULL, DELIMITER); // matrisin elemanlarını alıyor
         float emb = strtof(field,NULL); // float'a çeviriyor
         *(embeddings_matrix+i*EMBEDDING_DIMENSION+embIndex) = emb; // embedding matris array'inin içine bunu atıyor
      }
    }
    
    // Sends words to slaves
    MPI_Send(
        /* data         = */ words, 
        /* count        = */ wordCountPerProcess*MAX_WORD_LEN, 
        /* datatype     = */ MPI_CHAR,
        /* destination  = */ slaveRank, 
        /* tag          = */ 0, 
        /* communicator = */ MPI_COMM_WORLD);

    // Sends words' embedding matrix to slave
    MPI_Send(
        /* data         = */ embeddings_matrix, 
        /* count        = */ wordCountPerProcess*EMBEDDING_DIMENSION, 
        /* datatype     = */ MPI_FLOAT,
        /* destination  = */ slaveRank,
        /* tag          = */ 0,
        /* communicator = */ MPI_COMM_WORLD);
  }



  while(1 == 1){

    printf("Please type a query word:\n");
    char queryWord[256];
    scanf( "%s" , queryWord); // Takes query word from terminal
    printf("Query word:%s\n",queryWord);

   

    // Checks whether query word is equal to "EXIT", if so shut down the slave nodes
    if(strcmp(queryWord, "EXIT") == 0){
       for(int slaveRank=1; slaveRank<=numOfProcessor-1; slaveRank++){
       
          // Sends COMMAND_EXIT command to slaves
          MPI_Send(
                    /* data         = */ (void *)&COMMAND_EXIT, 
                    /* count        = */ 1, 
                    /* datatype     = */ MPI_INT, 
                    /* destination  = */ slaveRank,
                    /* tag          = */ 0, 
                    /* communicator = */ MPI_COMM_WORLD);

       }

       MPI_Finalize();
       exit(0);

      
    }
  
    // If query word is not equal to EXIT, proceeds the query word
    else{

      for(int slaveRank=1; slaveRank<=numOfProcessor-1; slaveRank++){
      
          // Sends COMMAND_QUERY command to slaves
          MPI_Send(
                    /* data         = */ (void *)&COMMAND_QUERY, 
                    /* count        = */ 1, 
                    /* datatype     = */ MPI_INT, 
                    /* destination  = */ slaveRank,
                    /* tag          = */ 0, 
                    /* communicator = */ MPI_COMM_WORLD);

          // Sends query word to slaves
          MPI_Send(
                    /* data         = */ queryWord, 
                    /* count        = */ MAX_WORD_LEN, 
                    /* datatype     = */ MPI_CHAR, 
                    /* destination  = */ slaveRank,
                    /* tag          = */ 0, 
                    /* communicator = */ MPI_COMM_WORLD);
      }

      int targetProcID = -1; // sets is -1, it means query word is not in "that" slave; if it finds the slave that query word is in, targetProcID will change 
     

      for(int slaveRank=1; slaveRank<=numOfProcessor-1; slaveRank++){
          int wordIndex;

          // Receives word index that is calculated in slave nodes
          MPI_Recv(
                /* data         = */ &wordIndex, 
                /* count        = */ 1, 
                /* datatype     = */ MPI_INT, 
                /* source       = */ slaveRank, 
                /* tag          = */ 0, 
                /* communicator = */ MPI_COMM_WORLD, 
                /* status       = */ MPI_STATUS_IGNORE);

          // If word index >= 0, it means query word is in one of the slaves
          if(wordIndex >= 0 ){
            targetProcID = slaveRank; // sets targetProcID to slaveRank whose slave contains the query word
          }

      }


      float qEmb[EMBEDDING_DIMENSION];
      float* queryEmbeddings = qEmb; // Keeps the embedding matrix of the query word
      
      // If targetProcID does not change, i mean -1, it means query word is not in the list
      // So, print Query word was not found.
      if(targetProcID <= 0 ){
        printf("Query word was not found.\n");
      }


      // The case that query word is found in one of the slaves (the rank of that slave is targetProcID)
      else{
       
        // Receives the query embedding matrix of the query word from the slave whose rank is equal to targetProcID 
        MPI_Recv(
                /* data         = */ queryEmbeddings, 
                /* count        = */ EMBEDDING_DIMENSION, 
                /* datatype     = */ MPI_FLOAT, 
                /* source       = */ targetProcID, 
                /* tag          = */ 0, 
                /* communicator = */ MPI_COMM_WORLD, 
                /* status       = */ MPI_STATUS_IGNORE);


        for (int slaveRank = 1; slaveRank <= numOfProcessor-1; slaveRank++){

          // To calculate the similarity, Sends COMMAND_CALCULATE_SIMILARITY to slaves 
          MPI_Send(
                    /* data         = */ (void *)&COMMAND_CALCULATE_SIMILARITY, 
                    /* count        = */ 1, 
                    /* datatype     = */ MPI_INT, 
                    /* destination  = */ slaveRank,
                    /* tag          = */ 0, 
                    /* communicator = */ MPI_COMM_WORLD);

          // To calculate the similarity, Sends embeddind matirx of the query word to slaves
           MPI_Send(
                    /* data         = */ queryEmbeddings, 
                    /* count        = */ EMBEDDING_DIMENSION, 
                    /* datatype     = */ MPI_FLOAT, 
                    /* destination  = */ slaveRank,
                    /* tag          = */ 0, 
                    /* communicator = */ MPI_COMM_WORLD);
         
        }
      
        char* similarWordArr  = (char*)malloc(sizeof(char) * (numOfProcessor-1)*MAX_WORD_LEN); // Keeps most similar words that will be received from slaves
        float* similarityScoreArr = (float*)malloc(sizeof(float)* numOfProcessor-1); // Keeps similarities that will be received from slaves
       
      
        for(int slaveRank = 1; slaveRank <= numOfProcessor-1; slaveRank++){
          char similarWord[MAX_WORD_LEN]; // Keeps word that will be received from slave
          float similarityScore; // Keeps similarity score that will be received from slave
          
          // Receives the similar word from slave
          MPI_Recv(similarWord, MAX_WORD_LEN, MPI_CHAR, slaveRank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
          strncpy(similarWordArr + (slaveRank-1) * MAX_WORD_LEN, similarWord, MAX_WORD_LEN); // Puts the word into similar word array 
     
          // Receives the similarity score from slave
          MPI_Recv(&similarityScore, 1, MPI_FLOAT, slaveRank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
          *(similarityScoreArr + (slaveRank-1)) = similarityScore; // Puts the score into similarity score array
    
        }

        for(int slaveRank=1; slaveRank <= numOfProcessor-1; slaveRank++){
          printf("%20s: ", similarWordArr + (slaveRank-1) * MAX_WORD_LEN); // print the similar words
          printf("%f\n", *(similarityScoreArr + slaveRank-1)); // print words' corresponding similarity scores
    
        }
      
      
        free(similarWordArr); // Frees the location
        free(similarityScoreArr); // Frees the location

        // 
      
      }
    }

  }
}



void runSlaveNode(int rank, int wordCountPerProcess){

  
  char* words = (char*)malloc(sizeof(char) * wordCountPerProcess*MAX_WORD_LEN); // array that will keep the words that is sent by master
  MPI_Recv(words, wordCountPerProcess*MAX_WORD_LEN, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // Receives the words from master

  float* embeddings_matrix = (float*)malloc(sizeof(float) * wordCountPerProcess*EMBEDDING_DIMENSION); // array that will keep the embedding matrices that is sent by master
  MPI_Recv(embeddings_matrix, wordCountPerProcess*EMBEDDING_DIMENSION, MPI_FLOAT, 0, 0, MPI_COMM_WORLD,MPI_STATUS_IGNORE); // Receives the embedding matrices from master

  while(1==1){
  
    int command;
    // Receives the command sent by master
    MPI_Recv(
          /* data         = */ &command, 
          /* count        = */ 1, 
          /* datatype     = */ MPI_INT, 
          /* source       = */ 0, 
          /* tag          = */ 0, 
          /* communicator = */ MPI_COMM_WORLD, 
          /* status       = */ MPI_STATUS_IGNORE);
   
    
    // If command is COMMAND_EXIT, finalize the procedure
    if(command == COMMAND_EXIT){
      MPI_Finalize();
      exit(0);
    }


    // If command is COMMAND_QUERY, receives queryword from master and does some calculation
    else if(command == COMMAND_QUERY){

        char* queryWord = (char*)malloc(sizeof(char) * MAX_WORD_LEN); // Keeps query word

        // Receives query word from master
        MPI_Recv(
          /* data         = */ queryWord, 
          /* count        = */ MAX_WORD_LEN, 
          /* datatype     = */ MPI_CHAR, 
          /* source       = */ 0, 
          /* tag          = */ 0, 
          /* communicator = */ MPI_COMM_WORLD, 
          /* status       = */ MPI_STATUS_IGNORE);

        int wordIndex = findWordIndex(words, queryWord); // Calculates the index of the given word and declares it to wordIndex
     
        // Sends wordIndex to master
        MPI_Send(&wordIndex, 1, MPI_INT, 0,0, MPI_COMM_WORLD); 

        
        float* queryEmbeddings =  (embeddings_matrix+wordIndex*EMBEDDING_DIMENSION); // Embedding matrix of the query word.

        // If wordIndex >=0, it means, query word is in the slave
        if(wordIndex >= 0 ){

              // Sends embedding matrix of the query word
              MPI_Send(
                    /* data         = */ queryEmbeddings, 
                    /* count        = */ EMBEDDING_DIMENSION, 
                    /* datatype     = */ MPI_FLOAT, 
                    /* destination  = */ 0,
                    /* tag          = */ 0, 
                    /* communicator = */ MPI_COMM_WORLD);
        }

    }  


    // If command is COMMAND_CALCULATE_SIMILARITY, receives query embeddings from master and does some calculation
    else if(command == COMMAND_CALCULATE_SIMILARITY){
          
        float qEmb[EMBEDDING_DIMENSION];
        float* queryEmbeddings = qEmb; //embedding matrix of the query word
       
        // Receives query embeddings from master
        MPI_Recv(
          /* data         = */ queryEmbeddings, 
          /* count        = */ EMBEDDING_DIMENSION, 
          /* datatype     = */ MPI_FLOAT, 
          /* source       = */ 0, 
          /* tag          = */ 0, 
          /* communicator = */ MPI_COMM_WORLD, 
          /* status       = */ MPI_STATUS_IGNORE);

        
        int mostSimilarWordIndex = -1; // Sets it into -1, it will change when the most similar word is found to most similar word's index
        float maxSimilarityScore = -1; // Sets it into -1, it will change when the similarities are calculated and most is found.

        // Calculates the similarities word by word and returns the result in maxSimilarityScore and mostSimilarWordIndex
        for (int wordIndex = 0; wordIndex < wordCountPerProcess; wordIndex++){
            float similarity = 0.0;
   

            for(int embIndex = 0; embIndex<EMBEDDING_DIMENSION; embIndex++){
              float emb1 = *(queryEmbeddings + embIndex);
              float emb2 = *(embeddings_matrix + wordIndex*EMBEDDING_DIMENSION + embIndex);
              similarity +=(emb1*emb2);
            }
   

            if(similarity > maxSimilarityScore){
              maxSimilarityScore = similarity;
              mostSimilarWordIndex = wordIndex;
            }
        }
          
          // Sends the most similar word to master
          MPI_Send(
                    /* data         = */ (void *)(words + mostSimilarWordIndex*MAX_WORD_LEN), 
                    /* count        = */ MAX_WORD_LEN, 
                    /* datatype     = */ MPI_CHAR, 
                    /* destination  = */ 0,
                    /* tag          = */ 0, 
                    /* communicator = */ MPI_COMM_WORLD);

          // Sends the max similarity score
          MPI_Send(
                    /* data         = */ &maxSimilarityScore, 
                    /* count        = */ 1, 
                    /* datatype     = */ MPI_FLOAT, 
                    /* destination  = */ 0,
                    /* tag          = */ 0, 
                    /* communicator = */ MPI_COMM_WORLD);
      
    }

  }

}


int main(int argc, char *argv[]){

  int size;
  int rank; 

  //Initialize the MPI env.
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  int slaveSize = size-1;

  if(slaveSize < 1){
    if(rank==0){
      printf("At least 1 slave processor is needed.\n");
    }
    MPI_Finalize();
    exit(0);
  }

  // Master node's rank is 0.
  if(rank == 0){
    runMasterNode(rank, NUM_WORDS/slaveSize, slaveSize+1);
  }
  
  // Other ranks for slave
  else{
    runSlaveNode(rank, NUM_WORDS/slaveSize);
  }

  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();

  return 0;
}