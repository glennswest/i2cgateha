struct qentry_struct {
       struct qentry_struct *next;
       struct qentry_struct *prev;
       };
struct queue_struct {
     struct qentry_struct  *head;
     struct qentry_struct  *tail;
     };

void queue_init(struct queue_struct *);
void queue_head(struct queue_struct *, struct qentry_struct *);


