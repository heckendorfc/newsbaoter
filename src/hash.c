#include <string.h>

#include "hash.h"
#include "common_defs.h"

static int urltag_tab_cap;
static int urltag_tab_size;
static var_t **urltag_tab;
static var_t deleted;

uint32_t hash_func(const char *str){
	uint32_t k;
	const char *p=str;
	for(k=0;*p;p++){
		k += *p;
		k += (k<<10);
		k ^= (k>>6);
	}
	k += (k<<3);
	k ^= (k>>11);
	k += (k<<15);

	return k;
}

static void grow_hash(var_t ***tab,int *size){
	var_t **temp=*tab;
	int i,j;
	uint32_t k;
	int new_size=2*(*size);

	INIT_MEM(*tab,new_size);
	for(i=0;i<new_size;i++)(*tab)[i]=NULL;

	for(i=0;i<*size;i++){
		if(temp[i]==NULL || temp[i]==&deleted)continue;

		k=hash_func(temp[i]->name);

		for(j=k%new_size;(*tab)[j]!=NULL;j=(j+1+k)%new_size);
		(*tab)[j]=temp[i];
	}

	*size=new_size;
}

static int find_hash(const char *n, uint32_t k, var_t **tab, int size){
	int i;
	int index;
	int ret=-1;

	for(i=0;i<size;i++){
		index=(i+k)%size;
		if(tab[index]==&deleted){
			if(ret<0)
				ret=index;
			continue;
		}
		if(tab[index]==NULL){
			if(ret<0)
				ret=index;
			break;
		}
		if(strcmp(tab[index]->name,n)==0)
			return index;
	}

	return ret;
}

void set_hash_var(const char *name, void *value, var_t **tab, int *size, int *cap){
	uint32_t k;
	int index;

	if(*cap>(*size/4)*3)
		grow_hash(&tab,size);

	k=hash_func(name);
	index=find_hash(name,k,tab,*size);
	if(tab[index]==NULL || tab[index]==&deleted){
		INIT_MEM(tab[index],1);
		tab[index]->name=strdup(name);
		tab[index]->value=NULL;
	}
	if(tab[index]==NULL){
		(*cap)++;
	}
	tab[index]->value=value;
}

void unset_hash_var(const char *name, var_t **tab, int *size, int *cap){
	uint32_t k;
	int index;

	k=hash_func(name);
	index=find_hash(name,k,tab,*size);
	if(tab[index]==NULL || tab[index]==&deleted){
		return;
	}
	(*cap)--;
	free(tab[index]);
	tab[index]=&deleted;
}

void* get_hash_var(const char *name, var_t **tab, int size){
	uint32_t k;
	int index;

	if(!name)return NULL;

	k=hash_func(name);
	index=find_hash(name,k,tab,size);
	if(tab[index]==NULL || tab[index]==&deleted){
		fprintf(stderr,"%s: Undefined variable.\n",name);
		return NULL;
	}
	return tab[index]->value;
}

/* Table specific functions */

void init_urltag_table(){
	int i;
	INIT_MEM(urltag_tab,INITIAL_LOCTAB_SIZE);
	urltag_tab_size=INITIAL_LOCTAB_SIZE;
	urltag_tab_cap=0;
	for(i=0;i<urltag_tab_size;i++)urltag_tab[i]=NULL;
}

void set_urltag(const char *name, void *value){
	set_hash_var(name,value,urltag_tab,&urltag_tab_size,&urltag_tab_cap);
}

void* get_urltag(const char *name){
	return get_hash_var(name,urltag_tab,urltag_tab_size);
}

/*
int is_alias(const char *name){
	uint32_t k;
	int index;

	if(!name)return 0;

	k=hash(name);
	index=find_urltag(name,k,alias_tab,alias_tab_size);

	return (alias_tab[index]==NULL || alias_tab[index]==&deleted)?0:1;
}
*/
