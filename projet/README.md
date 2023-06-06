# Gestion de files d'attentes

## Compilation

Pour le bon fonctionnement du programme, veillez dans un premier temps compiler et exécuter le fichier `dispatcher.c`. Les fichiers `client.c`et `guichet.c`ont besoin du pid du dispatcher pour pouvoir s'exécuter corretement. `guichet.c` est le deuxième à devoir être compilé et exécuté, et enfin le fichier `client.c`. Les guichets ont besoins d'être initialisé pour pouvoir traiter les demandes des clients.

## Terminaison

Lorsque toutes les requêtes de tous les paquets auront reçu leur réponse, le processus `client` se terminera automatiquement.
Pour terminer `dispatcher` et `guichet` veuillez faire ctrl+c, un handler fait pour a été implémenté permettant de cloturer l'utilisation des différents
élements dans ces fichiers.

## Eléments non implémentés

Par manque de temps, l'implémentation de différents clients et guichets à l'aide de threads n'a pas pu être effectuées.
