#ifndef FLOWER_H
#define FLOWER_H

enum state {
  WATERED,
  NEED_WATER,
  OVERWATERED,
  DEAD
};

struct Flower {
  unsigned int id;
  unsigned int time_to_water;
  unsigned int time_without_water;
  enum state st;
  int in_queue;
};

#endif