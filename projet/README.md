# Gestion de files d'attentes

## Compilation et exécution

Pour le bon fonctionnement du programme, veillez dans un premier temps compiler et exécuter le fichier `dispatcher.c`. Les fichiers `client.c` et `guichet.c`ont besoin du pid du dispatcher pour pouvoir s'exécuter corretement. `guichet.c` est le deuxième à devoir être compilé et exécuté, et enfin le fichier `client.c`. Les guichets ont besoins d'être initialisé pour pouvoir traiter les demandes des clients.

Par défaut, j'ai posé le nombre de requêtes à 4 et le nombre de paquets à 2 pour une meilleur lisibilité. Pour utiliser les ranges complets de ces deux paramètres,
veillez, dans le fichier `client.c`, décommenter la ligne 115 et commenter la ligne 116 pour le nombre de requêtes, et décommenter la ligne 111 et commenter la ligne 112 pour le nombre de paquets.

## Terminaison

Lorsque toutes les requêtes de tous les paquets auront reçu leur réponse, le processus `client` se terminera automatiquement.
Pour terminer `dispatcher` et `guichet` veuillez faire ctrl+c, un handler fait pour a été implémenté permettant de cloturer l'utilisation des différents élements dans ces fichiers.

## Eléments non implémentés

Par manque de temps, l'implémentation de différents clients et guichets à l'aide de threads n'a pas pu être effectuées.
