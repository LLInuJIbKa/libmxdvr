#ifndef QUEUE_H_
#define QUEUE_H_


typedef struct queue* queue;

queue queue_new(int buffer_size, int queue_size);
void queue_delete(queue* q);
void queue_push(queue q, unsigned char* data);
unsigned char* queue_pop(queue q);



#endif /* QUEUE_H_ */
