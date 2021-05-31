import re

from typing import Union


def is_valid_keyname(key: str) -> bool:
    pattern = re.compile(r'^[^:/\[\] ][^/\[\] ]*$')
    return pattern.match(key) is not None


def convert(config: Union[int, float, str, bool, dict], root='') -> str:
    res = []

    if isinstance(config, bool):  # Must be before int, bool is also int
        res.append(f'{root}/:type bool')
        res.append(f'{root}/ {1 if config else 0}')
    elif isinstance(config, (int, float)):
        res.append(f'{root}/:type num')
        res.append(f'{root}/ {config}')
    elif isinstance(config, str):
        res.append(f'{root}/:type str')
        res.append(f'{root}/ {config}')
    elif isinstance(config, dict):
        # Check duplicated keys during all keys are converted to str
        if len(config.keys()) != len(set(map(str, config.keys()))):
            raise ValueError('Same keyname as str is not allowed')
        for key in config.keys():
            if not is_valid_keyname(key):
                raise ValueError(f'Key name "{key}" is not allowed')

        res.append(f'{root}/:type dict')
        res.append(f'{root}/:length {len(config.keys())}')
        for index, (key, val) in enumerate(sorted(config.items())):
            res.append(f'{root}/:keys[{index}] {key}')
            res.extend(convert(val, f'{root}/{key}'))
    elif isinstance(config, list):
        res.append(f'{root}/:type list')
        res.append(f'{root}/:length {len(config)}')
        for index, val in enumerate(config):
            res.extend(convert(val, f'{root}[{index}]'))

    return res if root else '\n'.join(res)


def test():
    testdict = {
        'users': [
            {
                'name': 'Jeong Arm',
                'email': 'jarm@802.11ac.net',
            },
        ],
        'log_level': 'debug',
        'promisc': True,
        'pie': 6.28,
        'ids': [1, 2],
    }

    answer = {
        '/:type dict',
        '/:length 5',
        '/:keys[0] ids',
        '/:keys[1] log_level',
        '/:keys[2] pie',
        '/:keys[3] promisc',
        '/:keys[4] users',
        '/users/:type list',
        '/users/:length 1',
        '/users[0]/:type dict',
        '/users[0]/:length 2',
        '/users[0]/:keys[0] email',
        '/users[0]/:keys[1] name',
        '/users[0]/name/:type str',
        '/users[0]/name/ Jeong Arm',
        '/users[0]/email/:type str',
        '/users[0]/email/ jarm@802.11ac.net',
        '/log_level/:type str',
        '/log_level/ debug',
        '/promisc/:type bool',
        '/promisc/ 1',
        '/pie/:type num',
        '/pie/ 6.28',
        '/ids/:type list',
        '/ids/:length 2',
        '/ids[0]/:type num',
        '/ids[0]/ 1',
        '/ids[1]/:type num',
        '/ids[1]/ 2',
    }

    converted = set(convert(testdict).split('\n'))
    if converted != answer:
        print('Expected but not found: ', answer - converted)
        print('Not expected: ', converted - answer)
        return 1

    return 0


if __name__ == '__main__':
    import sys

    if len(sys.argv) == 2:
        import yaml

        with open(sys.argv[1]) as f:
            config = yaml.load(f, Loader=yaml.FullLoader)
            print(convert(config))
    else:
        sys.exit(test())
