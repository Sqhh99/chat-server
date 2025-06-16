create table users
(
    id              serial
        primary key,
    username        varchar(50)             not null
        unique,
    email           varchar(100)            not null
        unique,
    password        varchar(100)            not null,
    avatar          varchar(255),
    verified        boolean   default false,
    last_login_time timestamp,
    online          boolean   default false,
    create_time     timestamp default now() not null
);

alter table users
    owner to sqhh99;

create table verification_codes
(
    id          serial
        primary key,
    user_id     integer
        references users,
    email       varchar(100)            not null,
    code        varchar(10)             not null,
    create_time timestamp default now() not null,
    expire_time timestamp               not null,
    used        boolean   default false
);

alter table verification_codes
    owner to sqhh99;

create index idx_verification_codes_email
    on verification_codes (email);

create index idx_verification_codes_user_id
    on verification_codes (user_id);

create table chats
(
    id                serial
        primary key,
    name              varchar(100),
    type              smallint                not null,
    avatar            varchar(255),
    owner_id          integer
        references users,
    create_time       timestamp default now() not null,
    last_message_time timestamp
);

alter table chats
    owner to sqhh99;

create table sessions
(
    id          serial
        primary key,
    user_id     integer
        references users,
    token       varchar(255)            not null,
    ip          varchar(50),
    device      varchar(100),
    create_time timestamp default now() not null,
    expire_time timestamp               not null,
    active      boolean   default true
);

alter table sessions
    owner to sqhh99;

create index idx_sessions_user_id
    on sessions (user_id);

create index idx_sessions_token
    on sessions (token);

create table private_messages
(
    id           serial
        primary key,
    from_user_id integer                                                       not null,
    to_user_id   integer                                                       not null,
    content      text                                                          not null,
    timestamp    timestamp with time zone default CURRENT_TIMESTAMP            not null,
    message_type varchar(20)              default 'private'::character varying not null,
    is_read      boolean                  default false                        not null,
    is_deleted   boolean                  default false                        not null,
    media_type   varchar(20)              default NULL::character varying,
    media_url    text,
    created_at   timestamp with time zone default CURRENT_TIMESTAMP            not null
);

alter table private_messages
    owner to sqhh99;

create index idx_private_messages_from_user
    on private_messages (from_user_id);

create index idx_private_messages_to_user
    on private_messages (to_user_id);

create index idx_private_messages_timestamp
    on private_messages (timestamp);

create index idx_private_messages_users
    on private_messages (from_user_id, to_user_id);

create table group_messages
(
    id           serial
        primary key,
    group_id     integer                                                     not null,
    from_user_id integer                                                     not null,
    content      text                                                        not null,
    timestamp    timestamp with time zone default CURRENT_TIMESTAMP          not null,
    message_type varchar(20)              default 'group'::character varying not null,
    is_deleted   boolean                  default false                      not null,
    media_type   varchar(20)              default NULL::character varying,
    media_url    text,
    created_at   timestamp with time zone default CURRENT_TIMESTAMP          not null
);

alter table group_messages
    owner to sqhh99;

create index idx_group_messages_group
    on group_messages (group_id);

create index idx_group_messages_from_user
    on group_messages (from_user_id);

create index idx_group_messages_timestamp
    on group_messages (timestamp);

create table groups
(
    id          serial
        primary key,
    name        varchar(100)                                       not null,
    creator_id  integer                                            not null,
    description text,
    avatar_url  text,
    created_at  timestamp with time zone default CURRENT_TIMESTAMP not null,
    updated_at  timestamp with time zone default CURRENT_TIMESTAMP not null
);

alter table groups
    owner to sqhh99;

create table group_members
(
    id        serial
        primary key,
    group_id  integer                                                      not null,
    user_id   integer                                                      not null,
    role      varchar(20)              default 'member'::character varying not null,
    joined_at timestamp with time zone default CURRENT_TIMESTAMP           not null,
    unique (group_id, user_id)
);

alter table group_members
    owner to sqhh99;

create index idx_group_members_group
    on group_members (group_id);

create index idx_group_members_user
    on group_members (user_id);

create table user_friends
(
    id         serial
        primary key,
    user_id1   integer                                                        not null
        references users (id) on delete cascade,
    user_id2   integer                                                        not null
        references users (id) on delete cascade,
    status     varchar(20)              default 'accepted'::character varying not null,
    created_at timestamp with time zone default CURRENT_TIMESTAMP             not null,
    updated_at timestamp with time zone default CURRENT_TIMESTAMP             not null,
    unique (user_id1, user_id2)
);

alter table user_friends
    owner to sqhh99;

create index idx_user_friends_user1
    on user_friends (user_id1);

create index idx_user_friends_user2
    on user_friends (user_id2);

create index idx_user_friends_status
    on user_friends (status);

