#include "wrapper.h"





/*                                 MEMORY
 *    +-------------------------------------------------------------+
 *    |   #0 DATA BLOCK  |  #1 DATA BLOCK |   ...   | #N DATA BLOCK |
 *    +-------------------------------------------------------------+
 *
 *
 *                               DATA BLOCK
 *            +-------------+-----------------------------+
 *            |    HEADER   |          DATA               |
 *            +-------------+-----------------------------+
 *
 *
 * DATA   - bajty pre zapisovanie dat pre dane alokacie
 * HEADER - #1 bit je flag FREE 0/1 ci je dana alokacia volna alebo nie
 *			#2 bit je flag cislo velkosti alokovanej pamete je 2bajtove alebo 4bajtove cislo (16 alebo 32bit velkosti headerov)
 *          #N-2 bitov je velkost bloku
 *
 *
 * POZN   - tie prve dva bity nam nebudu chybat! kedze vieme ze 2^30 je najvacsia pamat 
 * FREE FLAG - 0=FREE, 1=ALLOCATED
 * SIZE FLAG - 0=2bajty 1=4bajty
 *
 *
 * 
 */


/*
 * inicializujeme prvy blok ktory je free o velkosti pamete
 * minus pocet bajtov ktore sme pouzili na zapis tejto infromacie
 */
void my_init(void) {

	uint8_t shifts[4] = {0, 8, 16, 24};
	unsigned int bytes = msize() > 16383 ? 4 : 2; //2^14 a dva bity na FLAGY
	for(int i = 0; i < bytes; i++){
		mwrite(i, (msize()-bytes) >> shifts[bytes - i - 1]);
	}

	uint8_t firstByte = mread(0);
	if(bytes == 4){
		firstByte ^= (-1 ^ firstByte) & (1 << 6);
	}else{
		firstByte ^= (-0 ^ firstByte) & (1 << 6);
	}
	mwrite(0, firstByte);
	return;
}

/*
 * Prechadzame celou pametou "skakavo ako linked list keby"
 * najrpv sa pozreme na prvy bit vzdy aky ma flag a precitame
 * velkost dat a podla FLAGU bud alokujeme alebo skocime na 
 * najblizsi block dat ak sa nenaslo dostatok miesta vratime FAIL
 * v opacnom pripade OK
 */
int my_alloc(unsigned int size) {

	if(size >= msize()-2 || size <= 0){ //-2 preto lebo najmensia mozna hlavicka je o velkosti 2bajtov
		return FAIL;
	}

	uint8_t shifts[4] = {0, 8, 16, 24};
	unsigned int index = 0, numberBytes = 0, block_size = 0,
	 		local_minimum = 1073741824, local_minimum_index = -1, local_block_size = 0;
	uint8_t firstByte = 0;

	while(index < msize()-3){
		firstByte = mread(index);
		numberBytes = (firstByte & (1 << (6))) ? 4 : 2; // _ ? _ _ _ _ _ _
		block_size = 0;

		for(int i =0; i < numberBytes; i++){ //nacitame z hlavicky velkost bloku
			block_size |= (mread(index + i)) << shifts[numberBytes - i - 1];
		}

		block_size ^= (-0 ^ block_size) & (1 << (numberBytes*8-1)); //nulujeme FREE FLAG bit
		block_size ^= (-0 ^ block_size) & (1 << (numberBytes*8-2)); //nulujeme SIZE FLAG bit

		//je blok free a dostatocne velky ? ak ano upozreme sa ci je aj najmensi mozny blok ktory mozeme pouzit
		if((firstByte & (1<<(7))) == 0 && block_size >= size && (block_size - size) < local_minimum){
			local_minimum = (block_size - size);
			local_minimum_index = index;
			local_block_size = block_size;
		}

		index += (block_size + numberBytes); //posuvame sa na dalsi datovy blok
	}

	if(local_minimum_index == -1){ //nenasli sme ziaden dostatocne velky pristor v pamati
		return FAIL;
	}


	unsigned int bytes = 0;

	//ak zvysna pamet blocku ktory ideme pouzit
	//by mala ostat najviac 2bajty tak sa nam to neoplati
	//pretoze to je miminalna velkost headera cize ich rovno pripojime
	// k tomuto blocku ktory bude mat o najviac 2bajty viac 
	if(local_minimum > 2){

		bytes = size > 16383 ? 4 : 2; //2^14 -1 a dva bity na FLAGY
		for(int i = 0; i < bytes; i++){
			mwrite(local_minimum_index + i, (size) >> shifts[bytes - i - 1]);
		}

		firstByte = mread(local_minimum_index);
		firstByte ^= (-1 ^ firstByte) & (1 << 7);

		if(bytes == 4){
			firstByte ^= (-1 ^ firstByte) & (1 << 6);
		}else{
			firstByte ^= (-0 ^ firstByte) & (1 << 6);
		}
		mwrite(local_minimum_index, firstByte);

		//next free block nastavime 
		numberBytes = bytes;
		unsigned int new_block_size = (local_block_size - size - bytes);
		if(new_block_size > 16383){
			numberBytes = 4;
		}else{
			numberBytes = 2;
			if(bytes == 4){
				new_block_size += 2;
			}
		}

		for(int i =0; i < numberBytes; i++){
			mwrite(local_minimum_index + i + size + bytes, (new_block_size) >> shifts[numberBytes - i - 1]);
		}

		firstByte = mread(local_minimum_index + size + bytes);
		firstByte ^= (-0 ^ firstByte) & (1 << 7);

		if(numberBytes == 4){
			firstByte ^= (-1 ^ firstByte) & (1 << 6);
		}else{
			firstByte ^= (-0 ^ firstByte) & (1 << 6);
		}

		mwrite(local_minimum_index + size + bytes, firstByte);

	}else{
		numberBytes = local_block_size > 16383 ? 4 : 2; //2^14 a dva bity na FLAGY
		bytes = numberBytes;
		for(int i = 0; i < numberBytes; i++){
			mwrite(local_minimum_index + i, (local_block_size) >> shifts[numberBytes - i - 1]);
		}

		firstByte = mread(local_minimum_index);
		firstByte ^= (-1 ^ firstByte) & (1 << 7);

		if(numberBytes == 4){
			firstByte ^= (-1 ^ firstByte) & (1 << 6);
		}else{
			firstByte ^= (-0 ^ firstByte) & (1 << 6);
		}
		mwrite(local_minimum_index, firstByte);
	}

	return local_minimum_index + bytes;
}


/*
 * Prejdeme celou pametou a ak najdeme danu platnu adresu 
 * zmenime v headri prvy bit - FLAG FREE a mergujeme vsetky najblizsie 
 * zlava a sprava volne datove bloky cim vznikne jeden velky a vratime OK
 * v opacnom pripade ak prejdeme celou pametou a danu adresu 
 * nenajdeme vratime FAIL 
 */
int my_free(unsigned int addr) {

	int8_t shifts[4] = {0, 8, 16, 24};

	unsigned int index = 0, block_size = 0, 
		numberBytes = 0, merge_index = -1,
		merge_block = 0, originalBytes;
	uint8_t firstByte = 0;

	while(index < msize()-2){
		firstByte = mread(index);
		block_size = 0;
		numberBytes = (firstByte & (1 << (6))) ? 4 : 2; 

		if(merge_index == -1 && (firstByte & (1 << (7))) == 0){
			merge_index = index;
		}

		for(int i =0; i < numberBytes; i++){
			block_size |= (mread(index + i)) << shifts[numberBytes - i - 1];
		}

		block_size ^= (-0 ^ block_size) & (1 << (numberBytes*8-1)); //nulujeme FREE FLAG bit
		block_size ^= (-0 ^ block_size) & (1 << (numberBytes*8-2)); //nulujeme SIZE FLAG bit

		if(firstByte & (1 << (7)) && (index+numberBytes) == addr){
			if(merge_index == -1){
				merge_index = index;
			}
			originalBytes = numberBytes;

			firstByte ^= (-0 ^ firstByte) & (1 << 7); //set FREE FLAG
			mwrite(index, firstByte);

			index = merge_index;
			block_size = 0;

			while(index < msize()-3 && (mread(index)& (1 << (7))) == 0){
				firstByte = mread(index);
				numberBytes = (firstByte & (1 << (6))) ? 4 : 2; 
				merge_block = 0;

				for(int i =0; i < numberBytes; i++){
					merge_block |= (mread(index + i)) << shifts[numberBytes - i - 1];
				}

				merge_block ^= (-0 ^ merge_block) & (1 << (numberBytes*8-1)); //nulujeme FREE FLAG bit
				merge_block ^= (-0 ^ merge_block) & (1 << (numberBytes*8-2)); //nulujeme SIZE FLAG bit

				if(block_size == 0){ //prvy krat
					block_size += (merge_block);
				}else{//musime potom zapocita aj numberBytes lebo znich sa stanu tym padom volne bajty
					block_size += (merge_block + numberBytes);
				}
				
				index += (merge_block + numberBytes);
			}

			//zapiseme velkost do pamate
			numberBytes = block_size > 16383 ? 4 : 2; //2^14 a dva bity na FLAGY
			if(originalBytes != numberBytes){ //plati ze numberBytes >= originalBytes ak plati ostra nerovnost 
				block_size -= 2;		      //musime odcitat 2bajty ktore pouzijeme do hlavicky bloku
			}

			for(int i =0; i < numberBytes; i++){
				mwrite(merge_index + i , (block_size) >> shifts[numberBytes - i - 1]);
			}

			firstByte = mread(merge_index);
			firstByte ^= (-0 ^ firstByte) & (1 << 7); //set FREE FLAG
			if(numberBytes == 4){
				firstByte ^= (-1 ^ firstByte) & (1 << 6);
			}else{
				firstByte ^= (-0 ^ firstByte) & (1 << 6);
			}
			mwrite(merge_index, firstByte);
			return OK;
		}

		if(firstByte & (1 << (7))){
			merge_index = -1;
		}

		index += (block_size + numberBytes);
	}

	return FAIL;
}
