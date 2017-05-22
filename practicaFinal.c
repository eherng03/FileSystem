#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/init.h>      /* Needed for the macros */
#include <linux/pagemap.h>   /* PAGE_CACHE_SIZE */
#include <linux/fs.h>        /* libfs stuff */
#include <asm/atomic.h>      /* atomic_t stuff */
#include <asm/uaccess.h>     /* copy_to_user */

#define TMPSIZE 80		/* Tamaño que va a tener el array de chars tmp*/
#define LFS_MAGIC 0x19980122

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eva Hernández GArcía");


/*Contadores de los dos ficheros iniciales.*/
static atomic_t counter1, counter2;
/*Contador global que van a usar todos los ficheros nuevos al crearse*/
atomic_t counterX;		


/**************************************************************************************************
 *                                     DECLARACION DE FUNCIONES A USAR                            *   
 **************************************************************************************************/
static int __init assoofs_init(void);

static struct dentry *assoofs_get_super(struct file_system_type *fst, int flags, const char *devname , void *data );
static int assoofs_fill_super(struct super_block *sb, void *data, int silent);

static struct inode *assoofs_make_inode(struct super_block *sb, int mode);

static void assoofs_create_files(struct super_block *sb, struct dentry *root);
static struct dentry *assoofs_create_file(struct super_block *sb , struct dentry *dir, const char *name , atomic_t *counter);
static struct dentry *assoofs_create_dir(struct super_block *sb, struct dentry *parent, const char *name);

static int assoofs_open(struct inode *inode, struct file *filp);
static ssize_t assoofs_read_file( struct file *filp , char *buf, size_t count, loff_t *offset);
static ssize_t assoofs_write_file( struct file *filp, const char *buf, size_t count, loff_t *offset);

static int assoofs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
static int assoofs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
static struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags);

static void __exit assoofs_exit(void);


/**************************************************************************************************
 *                                                    STRUCTS                                     *   
 **************************************************************************************************/

/* Tipo que vamos a usar para el sistema de ficheros. */
static struct file_system_type assoofs_type = {
	.owner = THIS_MODULE,
	.name = "assoofs" ,
	.mount = assoofs_get_super,
	.kill_sb = kill_litter_super,
};

 /* Operaciones de los ficheros. */
static struct file_operations assoofs_file_ops = {
	.open = assoofs_open,
	.read = assoofs_read_file,
	.write = assoofs_write_file,
};


/* Operaciones del sistema.*/
static struct super_operations assoofs_s_ops = {
	.statfs = simple_statfs,
	.drop_inode = generic_delete_inode ,
};

/* Operaciones de los inodos. */
static struct inode_operations assoofs_inode_ops = {
	.create = assoofs_create,
	.lookup = assoofs_lookup,
	.mkdir = assoofs_mkdir,
};

	

/**************************************************************************************************
 *         ASSOOFS_GET_SUPER (Funcion que se ejecuta cuando se registra el sistema de ficheros)   *   
 **************************************************************************************************/
static struct dentry *assoofs_get_super(struct file_system_type *fst, int flags,
	const char *devname , void *data )
{
	return mount_bdev(fst, flags, devname, data, assoofs_fill_super);
} 

/*************************************************************************************************
 *                ASSOOFS_FILL_SUPER (Funcion que configura los parametros del superbloque)      *   
 *************************************************************************************************/
 static int assoofs_fill_super(struct super_block *sb, void *data, int silent){

 	struct inode *root;
 	struct dentry *root_dentry;


	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
	sb->s_magic = LFS_MAGIC;
	sb->s_op = &assoofs_s_ops;

	/* Creamos el nodo raiz, con permisos de directorio. */
	root = assoofs_make_inode(sb, S_IFDIR | 0755 );

	if(!root){
		printk(KERN_INFO "Error al crear el inodo de la raiz.");
		return -EFAULT;
	}

	root->i_op = &assoofs_inode_ops;		/*(simple_dir_inode_operations)*/
	root->i_fop = &simple_dir_operations;

	/* Creamos la entrada al directorio raiz. */
	root_dentry = d_make_root(root);

	if(!root_dentry){
		printk(KERN_INFO "Error al crear la entrada al directorio raiz.");
		return -EFAULT;
	}

	sb->s_root = root_dentry;

	/* Creamos los ficheros que van a estar en nuestro filesystem. */
	assoofs_create_files(sb, root_dentry);
	return 0;
}


/*************************************************************************************************
 *                ASSOOFS_MAKE_INODE (Funcion que crea un inodo e inicializa sus parametros)     *   
 *************************************************************************************************/
static struct inode *assoofs_make_inode(struct super_block *sb, int mode){
	
	struct inode *ret;
	ret = new_inode(sb);

	if(ret){
		ret->i_mode = mode;
		ret->i_uid.val = ret->i_gid.val = 0;
		ret->i_blocks = 0;
		ret->i_atime = ret->i_mtime = ret->i_ctime = CURRENT_TIME;
	}
	return ret;
}


/*************************************************************************************************
 *         ASSOOFS_CREATE_FILES (Funcion que crea los ficheros pedidos en la parte obligatoria)  *   
 *************************************************************************************************/
static void assoofs_create_files(struct super_block *sb, struct dentry *root){
	
	struct dentry *subdirectorio;

	/*Creamos el fichero contador1. */
	atomic_set(&counter1, 0);		/* Inicializamos el contador del fichero contador1 a 0. */
	assoofs_create_file(sb, root, "contador1", &counter1);	/*Creamos en root el fichero contador1*/
	
	/* Creamos un subdirectorio dento de root (carpeta1). */
	subdirectorio = assoofs_create_dir(sb, root, "carpeta1");

	atomic_set(&counter2, 0);		/* Inicializamos el contador del fichero contador2 a 0. */
			
	if (subdirectorio){			/* Si se ha creado con exito... */
		/* Creamos el fichero contador2 en nuestro nuevo subdirectorio. */
		assoofs_create_file(sb, subdirectorio, "contador2", &counter2);	
	}else{
		printk(KERN_INFO "Error al crear la carpeta1.");
		return -EFAULT;
	}
}


/*************************************************************************************************
 *                ASSOOFS_CREATE_FILE (Funcion que crea un fichero)                              *   
 *************************************************************************************************/
 static struct dentry *assoofs_create_file(struct super_block *sb , struct dentry *dir,
  const char *name , atomic_t *counter){
	
	struct dentry *dentry;
	struct inode *inode;
	struct qstr qname;

	qname.name = name;
	qname.len = strlen(name);
	qname.hash = full_name_hash(name, qname.len);

	dentry = d_alloc(dir, &qname);
	if(!dentry){
		printk(KERN_INFO "Error al crear la entrada al directorio para el fichero.");
		return -EFAULT;
	}
		
	inode = assoofs_make_inode(sb, S_IFREG | 0644);
	if(!inode){
		printk(KERN_INFO "Error al crear el inodo para el fichero.");
		return -EFAULT;
	}

	inode->i_fop = &assoofs_file_ops;	/* Le asignamos las operaciones que puede realizar*/
	inode->i_private = counter;	/* En el campo i_private del inodo esta su contador. */

	//Añade la esctructura dentry a la cache de directorios
	d_add(dentry, inode);
	return dentry;		/* Devuelve la entrada al directorio */
}


/*************************************************************************************************
 *                ASSOOFS_CREATE_DIR (Funcion que crea un directorio)                            *   
 *************************************************************************************************/
static struct dentry *assoofs_create_dir(struct super_block *sb, 
	struct dentry *parent, const char *name)
{
	struct dentry *dentry;
	struct inode *inode;
	struct qstr qname;

	qname.name = name;
	qname.len = strlen (name);
	qname.hash = full_name_hash(name, qname.len);

	dentry = d_alloc(parent, &qname);
	if (!dentry){
		printk(KERN_INFO "Error al crear la entrada al subdirectorio.");
		return -EFAULT;
	}

	inode = assoofs_make_inode(sb, S_IFDIR | 0755); /*Le asigno permisos distintos que al fichero.*/
	if (!inode){
		printk(KERN_INFO "Error al crear el iniodo del subdirectoio. ");
		return -EFAULT;
	}
	inode->i_op = &assoofs_inode_ops;
	inode->i_fop = &simple_dir_operations;

	d_add(dentry, inode);

	return dentry;
}



/*************************************************************************************************
 *                            Operaciones de nuestro sistema de ficheros                         *   
 *************************************************************************************************/

//Abrir fichero
static int assoofs_open(struct inode *inode, struct file *filp){
	/* En inode->i_private es donde se almacena el contenido del fichero.*/
	filp->private_data = inode->i_private;
	return 0;
}

//Leer fichero
static ssize_t assoofs_read_file( struct file *filp , char *buf, size_t count, loff_t *offset){

	atomic_t *counter;
	int v, len;
	char tmp[TMPSIZE];

	counter = (atomic_t *) filp->private_data;

	v = atomic_read(counter);  	/* Sacamos el valor de counter del fichero */
	if (*offset > 0){
		v -= 1; 				/* Valor que devuelve cuando el offset es mayor que cero */
	}else{
		atomic_inc(counter);	
	}

	/* Pasa de int a la cadena de chars tmp, y almacena en len el numero de bytes escritos.*/
	len = snprintf(tmp, TMPSIZE, "%d\n", v);
	if(*offset > len){
		return 0 ;
	}

	if (count > len - *offset ){
		count = len - *offset;
	}

	/* Copia al espacio de usuario (concretamente a buff) el contenido de tmp
	 * que esta en el kernel
	 */
	if (copy_to_user(buf, tmp + *offset, count)){
		return -EFAULT;
	}

	*offset += count;			//Incrementa el count y lo devuelve
	return count;
}


//Escribir en el fichero
static ssize_t assoofs_write_file( struct file *filp, const char *buf,
	size_t count, loff_t *offset){

	atomic_t *counter;
	char tmp[TMPSIZE];

	counter = (atomic_t *) filp->private_data;

	if (*offset != 0){
		printk(KERN_INFO "offset != 0 al escribir \n");
		return -EINVAL;
	}

	if (count >= TMPSIZE){	/* Se sale del char array*/
		printk(KERN_INFO "offset != 0 al escribir \n");
		return -EINVAL;
	}

	memset(tmp, 0, TMPSIZE);		/*Rellena el array con 0*/
	/* Copia el contenido de buf (que esta en el espacio de usuario) a
	 * tmp (que esta en el kernel)
	 */
	if (copy_from_user(tmp, buf, count)){
		return -EFAULT;
	}

	/* Introduce en counter el valor que hay en tmp en base 10
	 */
	atomic_set(counter, simple_strtol(tmp, NULL, 10));
	return count;
}


/*************************************************************************************************
 *                             Operaciones que realizan los inodos                               *   
 *************************************************************************************************/
/*Buscar el fichero*/
static struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags)
{
	printk(KERN_INFO "assoofs_lookup\n");
	return NULL;
}


/* Crear directorio */
static int assoofs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode){

	struct inode *nuevoInodo = assoofs_make_inode(dir->i_sb, S_IFDIR | 0755);

	if (!nuevoInodo){
		printk(KERN_INFO "Error al crear el inodo para el nuevo directorio. ");
		return -EFAULT;
	}

	nuevoInodo->i_op = &assoofs_inode_ops;
	nuevoInodo->i_fop = &simple_dir_operations;

	d_add(dentry, nuevoInodo);

	return 0;
}


/*Crear un nuevo fichero*/
static int assoofs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl){

	struct inode *nuevoInodo;

	atomic_set(&counterX, 0);	/*Inicializo el contador a 0 */
	nuevoInodo = assoofs_make_inode(dir->i_sb, mode);

	if (!nuevoInodo){
		printk(KERN_INFO "Error al crear el iniodo del nuevo fichero. ");
		return -EFAULT;
	}

	nuevoInodo->i_fop = &assoofs_file_ops; 	/*Le asigno operaciones de fichero. */
	nuevoInodo->i_private = &counterX;

	d_add(dentry, nuevoInodo);	/*Añado el nuevo inodo al sistema. */
	return 0;
}


/*************************************************************************************************
 *                  Operaciones que realizan los inodos                                          *   
 *************************************************************************************************/
//Inicializacion el sistema de ficheros
static int __init assoofs_init(void)
{
	return register_filesystem(&assoofs_type);
}

//Salida del modulo
static void __exit assoofs_exit(void)
{
	//unregister_filesystem(&assoofs_type);
}
	

module_init(assoofs_init);
module_exit(assoofs_exit);


//Dentry representa una entrada al directorio
//Creacion de inodos hecha
//Campo iprivate del inodo esta el contador
//Filp es el descriptor de fichero
//Counter lo sacamos de flip.private
//Tmp cadena de caracteres
//strtol pasa de cadena de caracteres a numero
