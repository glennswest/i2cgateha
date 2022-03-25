struct qentry_struct {
       struct qentry_struct *next;
       struct qentry_struct *prev;
       };
struct queue_struct {
     struct qentry_struct  *head;
     struct qentry_struct  *tail;
     };

void queue_init(struct queue_struct *);
void queue(struct queue_struct *, struct qentry_struct *);
struct qentry_struct *unqueue(struct queue_struct *);

void queue_init(struct queue_struct *q)
{
    q->head = NULL;
    q->tail = NULL; 
}

void queue(struct queue_struct *q, struct qentry_struct *qe)
{
     if (q->head == NULL){
         q->head = qe;
         q->tail = qe;
         qe->next = NULL;
         qe->prev = NULL;
         return;
         }
     qe->prev = q->tail;
     qe->next = NULL;
     q->tail->next = qe;
     q->tail = qe;
}

struct qentry_struct *unqueue(struct queue_struct *q)
{
struct qentry_struct *qe;

     if (q->tail == NULL){
        return(NULL);
        }
     qe = q->tail;
     q->tail = qe->prev;
     if (q->tail == NULL){
        q->head = q->tail;
        }
     qe->next = NULL;
     qe->prev = NULL;
     return(qe);

}


