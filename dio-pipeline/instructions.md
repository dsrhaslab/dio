
## TAGS
    - deploy_dio
    - delete_dio

## ENV
    - run_all (boolean)


## Create DIO from scratch

```
$ ansible-playbook -u gsd dio_playbook.yml --tags deploy_dio -e run_all=true
```

## Just start DIO

```
$ ansible-playbook -u gsd dio_playbook.yml --tags deploy_dio
```

## Just stop DIO

```
$ ansible-playbook -u gsd dio_playbook.yml --tags delete_dio
```

## Delete completely DIO

```
$ ansible-playbook -u gsd dio_playbook.yml --tags delete_dio -e run_all=true
```
