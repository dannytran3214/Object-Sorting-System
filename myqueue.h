#ifndef MYQUEUE_H_
#define MYQUEUE_H_

typedef enum {
	STEEL = 0, BLACK = 1, WHITE = 2, ALUMINUM = 3, METAL = 4, ERROR = -1
} material;

typedef struct Node {
	struct Node* next;
	material Material;
}Node;

typedef struct m_buffer_t {
	material arr[8];    //This is a ring counter - because there is a maximum 
	int read_index;    //number of Nodes in between the sensors, it will work
	int write_index;
	unsigned int size;
}m_buffer_t;

typedef struct Q {
	Node* head;
	Node* tail;
	int size;
	m_buffer_t m_buffer;
}Q;


Q Q_new();
void Q_add(Q* q, material lightness);
material Q_read(Q* q);
material Q_peek(Q* q);
material read_metal_buffer(Q* q);
void write_metal_buffer(Q* q, material m);

#endif /* MYQUEUE_H_ */