#include "data-access/commons/daoFactory.h"
#include "data-access/dao/pg/pgDao.h"


Dao* daoFactory_create(int useDao) {
	Dao *dao = NULL;

	switch (useDao) {
	case 1:
		// PostgreSQL DAO
		dao = (Dao*) PgDao_new();
		break;
	default:
		break;
	}

	return dao;
}
