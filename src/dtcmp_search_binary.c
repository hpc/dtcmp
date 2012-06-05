/* Copyright (c) 2012, Lawrence Livermore National Security, LLC.
 * Produced at the Lawrence Livermore National Laboratory.
 * Written by Adam Moody <moody20@llnl.gov>.
 * LLNL-CODE-557516.
 * All rights reserved.
 * This file is part of the DTCMP library.
 * For details, see https://github.com/hpc/dtcmp
 * Please also read this file: LICENSE.TXT. */

#include "dtcmp_internal.h"

int DTCMP_Search_local_low_binary(
  const void* target,  /* IN  - buffer holding target key */
  const void* list,    /* IN  - buffer holding ordered list of key/satellite items to search */
  int low,             /* IN  - number of items in list (non-negative integer) */
  int high,            /* IN  - number of items in list (non-negative integer) */
  MPI_Datatype key,    /* IN  - datatype of key (handle) */
  MPI_Datatype keysat, /* IN  - datatype of key and satellite (handle) */
  DTCMP_Op cmp,        /* IN  - key comparison function (handle) */
  int* flag,           /* OUT - set to 1 if target is in list, 0 otherwise (integer) */
  int* index           /* OUT - lowest index in list where target could be inserted (integer) */
)
{
  /* assume that we won't find the target */
  *flag = 0;

  /* get size of an element so we can increase our pointer by the correct amount */
  MPI_Aint lb, extent;
  MPI_Type_get_extent(keysat, &lb, &extent);

  while (low <= high) {
    /* get a pointer to value in the middle of high and low */
    int mid = (high + low) / 2;
    const void* candidate = (char*)list + mid * extent;

    /* compare this value to our target */
    int result = dtcmp_op_eval(target, candidate, cmp);
    if (result == 0) {
      /* found an exact match, move the higher marker down */
      *flag = 1;
      high = mid;
      if (low == high) {
        /* if the low is also the same as the high we're done,
         * otherwise we keep looking in case we find a match that is lower */
        break;
      }
    } else if (result < 0) {
      /* the target is smaller than the midpoint,
       * so move the high marker down */
      high = mid - 1;
    } else {
      /* the target is larger than the midpoint,
       * so move the low marker up */
      low = mid + 1;
    }
  }

  /* set index to the lowest index where target
   * could be inserted into list and still be in order */
  if (*flag) {
    /* set index to high, which will be lowest index where target can be inserted
     * so it is in order (and first in the list if there are duplicates) */
    *index = high;
  } else {
    /* if we didn't find it, we need to increment the high index by one */
    *index = high + 1;
  }

  return DTCMP_SUCCESS;
}

int DTCMP_Search_local_high_binary(
  const void* target,  /* IN  - buffer holding target key */
  const void* list,    /* IN  - buffer holding ordered list of key/satellite items to search */
  int low,             /* IN  - number of items in list (non-negative integer) */
  int high,            /* IN  - number of items in list (non-negative integer) */
  MPI_Datatype key,    /* IN  - datatype of key (handle) */
  MPI_Datatype keysat, /* IN  - datatype of key and satellite (handle) */
  DTCMP_Op cmp,        /* IN  - key comparison function (handle) */
  int* flag,           /* OUT - set to 1 if target is in list, 0 otherwise (integer) */
  int* index           /* OUT - highest index in list after which target could be inserted (integer) */
)
{
  /* assume that we won't find the target */
  *flag = 0;

  /* get size of an element so we can increase our pointer by the correct amount */
  MPI_Aint lb, extent;
  MPI_Type_get_extent(keysat, &lb, &extent);

  while (low <= high) {
    /* get a pointer to value in the middle of high and low */
    int mid = (high + low) / 2 + ((high - low) & 0x1);
    const void* candidate = (char*)list + mid * extent;

    /* compare this value to our target */
    int result = dtcmp_op_eval(target, candidate, cmp);
    if (result == 0) {
      /* found an exact match, move the low marker up */
      *flag = 1;
      low = mid;
      if (low == high) {
        /* if the low is also the same as the high we're done,
         * otherwise we keep looking in case we find a match that is higher */
        break;
      }
    } else if (result < 0) {
      /* the target is smaller than the midpoint,
       * so move the high marker down */
      high = mid - 1;
    } else {
      /* the target is larger than the midpoint,
       * so move the low marker up */
      low = mid + 1;
    }
  }

  /* set index to the highest index after which the target
   * could be inserted into list and still be in order */
  if (*flag) {
    /* set index to low, which will be highest index after which the target can be inserted
     * so it is in order (and last in the list if there are duplicates) */
    *index = low;
  } else {
    /* if we didn't find it, we need to decrement the low index by one */
    *index = low - 1;
  }

  return DTCMP_SUCCESS;
}

/* calls DTCMP_Search_low for an ordered list of targets and returns each corresponding index */
int DTCMP_Search_local_low_list_binary(
  int num,             /* IN  - number of targets (integer) */
  const void* targets, /* IN  - array holding target keys */
  const void* list,    /* IN  - array holding ordered list of key/satellite items to search */
  int low,             /* IN  - number of items in list (non-negative integer) */
  int high,            /* IN  - number of items in list (non-negative integer) */
  MPI_Datatype key,    /* IN  - datatype of key (handle) */
  MPI_Datatype keysat, /* IN  - datatype of key and satellite (handle) */
  DTCMP_Op cmp,        /* IN  - key comparison function (handle) */
  int* indicies        /* OUT - lowest index in list where target could be inserted (integer) */
)
{
  /* get size of an element so we can increase our pointer by the correct amount */
  MPI_Aint target_lb, target_extent;
  MPI_Type_get_extent(key, &target_lb, &target_extent);

  /* search for the lowest index of the middle target */
  int flag, index;
  int mid = num / 2;
  const void* target = (char*)targets + mid * target_extent;
  DTCMP_Search_local_low_binary(target, list, low, high, key, keysat, cmp, &flag, &index);
  indicies[mid] = index;

  /* use this index as a split point to recursively search each half for remaining targets */
  if (index > high) {
    /* if the target is bigger than any item in the list,
     * the index returned from the lowest search will be one more than we have in our list */
    index = high;
  }

  /* recursively search the bottom half of our list for the bottom half of the remaining targets */
  if (mid > 0) {
    DTCMP_Search_local_low_list_binary(
      mid, targets, list, low, index,
      key, keysat, cmp, indicies
    );
  }

  /* recursively search the top half of our list for the top half of the remaining targets */
  int upper_index = mid + 1;
  int upper_count = num - upper_index;
  if (upper_count > 0) {
    DTCMP_Search_local_low_list_binary(
      upper_count, (char*)targets + upper_index * target_extent, list, index, high,
      key, keysat, cmp, indicies + upper_index
    );
  }
  
  return 0;
}
