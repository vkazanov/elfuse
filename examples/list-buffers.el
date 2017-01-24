(require 'seq)
(require 'rx)
(require 'subr-x)

(require 'elfuse)

(defvar list-buffers--posix-portable-filename-re
  (rx (one-or-more (in "-0-9A-Za-z._")) line-end))

(elfuse-define-op create (path)
  (message "CREATE: %s" path)
  (get-buffer-create (file-name-nondirectory path))
  0)

(elfuse-define-op readdir (path)
  (message "READDIR: %s" path)
  (unless (equal path "/")
    (signal 'elfuse-op-error elfuse-errno-ENOENT))
  (seq-concatenate 'vector
                   '("." "..")
                   (list-buffers--list-buffer-names)))

(elfuse-define-op getattr (path)
  (message "GETATTR: %s" path)
  (cond
   ((equal path "/")
    [dir 0])
   ((member (file-name-nondirectory path)
            (list-buffers--list-buffer-names))
    [file 0])
   (t (signal 'elfuse-op-error elfuse-errno-ENOENT))))

(defun list-buffers--list-buffer-names ()
  (thread-last (buffer-list)
    (seq-filter #'list-buffers--filter)
    (seq-map #'buffer-name)))

(defun list-buffers--filter (buf)
  (let ((name (buffer-name buf)))
    (string-match list-buffers--posix-portable-filename-re name)))
