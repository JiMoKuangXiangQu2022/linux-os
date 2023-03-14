#include <malloc.h>
#include "radix-tree.h"

int main(void)
{
#define MAX_ITEM_NUM 8192
	struct radix_tree_root root = RADIX_TREE_INIT(0);
	struct data_item { int value; } *items = NULL, *pitem;
	int i, ret;

	items = (struct data_item *)
				malloc(sizeof(struct data_item) * MAX_ITEM_NUM);
	if (!items) {
		printf("out of memory\n");
		return -1;
	}
	for (i = 0; i < MAX_ITEM_NUM; i++)
		items[i].value = i + 1;

	ret = radix_tree_insert(&root, 0, &items[0]);
	if (ret) {
		printf("insert item 0 error: ret = %d\n", ret);
		return -1;
	}

	ret = radix_tree_insert(&root, 1, &items[1]);
	if (ret) {
		printf("insert item 1 error: ret = %d\n", ret);
		return -1;
	}

	ret = radix_tree_insert(&root, 2, &items[2]);
	if (ret) {
		printf("insert item 2 error: ret = %d\n", ret);
		return -1;
	}

	ret = radix_tree_insert(&root, 64, &items[64]);
	if (ret) {
		printf("insert item 64 error: ret = %d\n", ret);
		return -1;
	}

	ret = radix_tree_insert(&root, 65, &items[65]);
	if (ret) {
		printf("insert item 65 error: ret = %d\n", ret);
		return -1;
	}
	ret = radix_tree_insert(&root, 127, &items[127]);
	if (ret) {
		printf("insert item 127 error: ret = %d\n", ret);
		return -1;
	}

	ret = radix_tree_insert(&root, 128, &items[128]);
	if (ret) {
		printf("insert item 128 error: ret = %d\n", ret);
		return -1;
	}

	ret = radix_tree_insert(&root, 4095, &items[4095]);
	if (ret) {
		printf("insert item 4095 error: ret = %d\n", ret);
		return -1;
	}

	ret = radix_tree_insert(&root, 4096, &items[4096]);
	if (ret) {
		printf("insert item 4096 error: ret = %d\n", ret);
		return -1;
	}

	pitem = (struct data_item *)radix_tree_lookup(&root, 0);
	if (!pitem) {
		printf("can't find item 0\n");
		return -2;
	}
	printf("item 0: %d\n", pitem->value);

	pitem = (struct data_item *)radix_tree_lookup(&root, 1);
	if (!pitem) {
		printf("can't find item 1\n");
		return -2;
	}
	printf("item 1: %d\n", pitem->value);

	pitem = (struct data_item *)radix_tree_lookup(&root, 2);
	if (!pitem) {
		printf("can't find item 2\n");
		return -2;
	}
	printf("item 2: %d\n", pitem->value);

	pitem = (struct data_item *)radix_tree_lookup(&root, 3);
	if (!pitem) {
		printf("<WARN> can't find item 3\n");
	} else {
		printf("item 3: %d\n", pitem->value);
	}

	pitem = (struct data_item *)radix_tree_lookup(&root, 64);
	if (!pitem) {
		printf("can't find item 64\n");
		return -2;
	}
	printf("item 64: %d\n", pitem->value);

	pitem = (struct data_item *)radix_tree_lookup(&root, 65);
	if (!pitem) {
		printf("can't find item 65\n");
		return -2;
	}
	printf("item 65: %d\n", pitem->value);

	pitem = (struct data_item *)radix_tree_lookup(&root, 127);
	if (!pitem) {
		printf("can't find item 127\n");
		return -2;
	}
	printf("item 127: %d\n", pitem->value);

	pitem = (struct data_item *)radix_tree_lookup(&root, 128);
	if (!pitem) {
		printf("can't find item 128\n");
		return -2;
	}
	printf("item 128: %d\n", pitem->value);

	pitem = (struct data_item *)radix_tree_lookup(&root, 1028);
	if (!pitem) {
		printf("<WARN> can't find item 1028\n");
	} else {
		printf("item 1028: %d\n", pitem->value);
	}

	pitem = (struct data_item *)radix_tree_lookup(&root, 4095);
	if (!pitem) {
		printf("can't find item 4095\n");
		return -2;
	}
	printf("item 4095: %d\n", pitem->value);

	pitem = (struct data_item *)radix_tree_lookup(&root, 4096);
	if (!pitem) {
		printf("can't find item 4096\n");
		return -2;
	}
	printf("item 4096: %d\n", pitem->value);

	radix_tree_delete(&root, );

	free(items);

	return 0;
}