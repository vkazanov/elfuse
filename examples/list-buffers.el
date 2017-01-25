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
    (signal 'elfuse-op-error elfuse-ENOENT))
  (seq-concatenate 'vector
                   '("." "..")
                   (list-buffers--list-buffer-names)))

(elfuse-define-op getattr (path)
  (message "GETATTR: %s" path)
  (let ((name (file-name-nondirectory path)))
    (cond
     ((equal path "/")
      [dir 0])
     ((member name
              (list-buffers--list-buffer-names))
      (vector 'file (buffer-size (get-buffer name))))
     (t (signal 'elfuse-op-error elfuse-ENOENT)))))

(elfuse-define-op read (path offset size)
  (message "READ: %s %d %d" path offset size)
  (if-let ((name (file-name-nondirectory path))
           (buf (get-buffer name)))
      (with-current-buffer buf
        (list-buffers--substring (buffer-string) offset size))
    (signal 'elfuse-op-error elfuse-ENOENT)))

(elfuse-define-op unlink (path)
  (message "UNLINK: %s" path)
  (if-let ((name (file-name-nondirectory path))
           (buf (get-buffer name)))
      (kill-buffer buf)
    (signal 'elfuse-op-error elfuse-ENOENT)))

(defun list-buffers--substring (str offset size)
  (cond
   ((> offset (seq-length str)) "")
   ((> (+ offset size) (seq-length str)) (seq-subseq str offset))
   (t (seq-subseq str offset (+ offset size)))))


(defun list-buffers--list-buffer-names ()
  (thread-last (buffer-list)
    (seq-filter #'list-buffers--posix-filename-p)
    (seq-map #'buffer-name)))

(defun list-buffers--posix-filename-p (buf)
  (string-match list-buffers--posix-portable-filename-re
                (buffer-name buf)))
