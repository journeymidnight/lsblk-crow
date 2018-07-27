#!/bin/bash
systemctl daemon-reload
systemctl start serve-blk
systemctl enable serve-blk
