package model

import "time"

type Flag struct {
	ID        uint64
	Challenge string
	Points    int
	UserID    uint64
	CreatedAt time.Time
	UpdatedAt time.Time
}
