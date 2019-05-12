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
