package model

import "time"

type Flag struct {
	ID        uint64
	Name      string
	Hint      string
	Value     string
	Points    int
	CreatedAt time.Time
	UpdatedAt time.Time
	DeletedAt *time.Time
}
