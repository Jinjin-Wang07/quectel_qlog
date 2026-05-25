#include "qlog.h"
#include <stdio.h>
#include <stdlib.h>

// 创建一个新的节点
Node* create_node(const char* filename)
{
    Node* new_node = (Node*)malloc(sizeof(Node));
    strcpy(new_node->log_file_name, filename);
    new_node->next = NULL;
    return new_node;
}

// 创建一个空队列
Queue* create_queue()
{
    Queue* queue = (Queue*)malloc(sizeof(Queue));
    queue->front = NULL;
    queue->rear = NULL;
    return queue;
}

// 入队操作
void enqueue(Queue* queue, const char* data)
{
    Node* new_node = create_node(data);
    if (queue->rear == NULL)
    {
        // 如果队列为空，新的节点即为头和尾
        queue->front = new_node;
        queue->rear = new_node;
    }
    else
    {
        // 将新的节点添加到队列尾部
        queue->rear->next = new_node;
        queue->rear = new_node;
    }
    qlog_dbg("Enqueued: %s\n", data);
}

// 出队操作
int dequeue(Queue* queue)
{
    if (queue->front == NULL)
    {
        qlog_dbg("Queue is empty, cannot dequeue.\n");
        return -1; // 表示队列为空
    }
    Node* temp = queue->front;
    queue->front = queue->front->next;
    if (queue->front == NULL)
    {
        queue->rear = NULL; // 如果队列变为空，更新队列尾
    }
    qlog_dbg("Dequeued: %s\n", temp->log_file_name);
    free(temp); // 释放节点的内存
    return 0;
}

// 查看队列头元素
char* peek(Queue* queue)
{
    if (queue->front == NULL)
    {
        qlog_dbg("Queue is empty, cannot peek.\n");
        return NULL; // 表示队列为空
    }
    return queue->front->log_file_name;
}

// 检查队列是否为空
int is_empty(Queue* queue) { return queue->front == NULL; }

// 释放队列占用的内存
void destroy_queue(Queue* queue)
{
    while (!is_empty(queue))
    {
        dequeue(queue);
    }
    free(queue);
}