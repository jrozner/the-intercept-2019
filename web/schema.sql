create table users (
    id int not null auto_increment,
    team_name varchar(254) unique not null,
    access_key varchar(254) unique not null,
    secret_key varchar(254) not null,
    serial varchar(254) unique not null,
    created_at datetime not null,
    updated_at datetime not null,
    deleted_at datetime,
    primary key(id)
);

create table messages (
    id int not null auto_increment,
    text text not null,
    sent_by int not null,
    sent_to int not null,
    seen boolean not null,
    created_at datetime not null,
    updated_at datetime not null,
    deleted_at datetime,
    foreign key (sent_by) references users(id),
    foreign key (sent_to) references users(id),
    primary key(id)
);

create table flags (
    id int not null auto_increment,
    name varchar(254) not null unique,
    hint text not null,
    value varchar(254) not null unique,
    points int not null,
    created_at datetime not null,
    updated_at datetime not null,
    deleted_at datetime,
    primary key(id)
);

create table user_flags (
    id int not null auto_increment,
    user_id int not null,
    flag_id int not null,
    foreign key (user_id) references users(id),
    foreign key (flag_id) references flags(id),
    unique (user_id, flag_id),
    primary key(id)
);
